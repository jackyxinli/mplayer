/*
 * codec.conf parser
 * by Szabolcs Berecz <szabi@inf.elte.hu>
 * (C) 2001
 */

#define DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "libvo/video_out.h"
#include "codec-cfg.h"

#ifdef DEBUG
#	define DBG(str, args...) printf(str, ##args)
#else
#	define DBG(str, args...) do {} while (0)
#endif

#define PRINT_LINENUM printf("%s(%d): ", cfgfile, line_num)

#define MAX_NR_TOKEN	16

#define MAX_LINE_LEN	1000

#define RET_EOF		-1
#define RET_EOL		-2

#define TYPE_VIDEO	0
#define TYPE_AUDIO	1

static int add_to_fourcc(char *s, char *alias, unsigned int *fourcc,
		unsigned int *map)
{
	int i, j, freeslots;
	char **aliasp;
	unsigned int tmp;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	freeslots = CODECS_MAX_FOURCC - i;
	if (!freeslots)
		goto err_out_too_many;

	aliasp = (alias) ? &alias : &s;
	do {
		tmp = *((unsigned int *) s);
		for (j = 0; j < i; j++)
			if (tmp == fourcc[j])
				goto err_out_duplicated;
		fourcc[i] = tmp;
		map[i] = *((unsigned int *) (*aliasp));
		s += 4;
		i++;
	} while ((*(s++) == ',') && --freeslots);

	if (!freeslots)
		goto err_out_too_many;
	if (*(--s) != '\0')
		return 0;
	return 1;
err_out_duplicated:
	printf("\nduplicated fourcc/format\n");
	return 0;
err_out_too_many:
	printf("\ntoo many fourcc/format...\n");
	return 0;
}

static int add_to_format(char *s, unsigned int *fourcc, unsigned int *fourccmap)
{
	int i, j;

	/* find first unused slot */
	for (i = 0; i < CODECS_MAX_FOURCC && fourcc[i] != 0xffffffff; i++)
		/* NOTHING */;
	if (i == CODECS_MAX_FOURCC) {
		printf("\ntoo many fourcc/format...\n");
		return 0;
	}

        fourcc[i]=fourccmap[i]=strtoul(s,NULL,0);
	for (j = 0; j < i; j++)
		if (fourcc[j] == fourcc[i]) {
			printf("\nduplicated fourcc/format\n");
			return 0;
		}

	return 1;
}

static int add_to_out(char *sfmt, char *sflags, unsigned int *outfmt,
		unsigned char *outflags)
{
	static char *fmtstr[] = {
		"YUY2",
		"YV12",
		"RGB8",
		"RGB15",
		"RGB16",
		"RGB24",
		"RGB32",
		"BGR8",
		"BGR15",
		"BGR16",
		"BGR24",
		"BGR32",
		NULL
	};
	static unsigned int fmtnum[] = {
		IMGFMT_YUY2,
		IMGFMT_YV12,
		IMGFMT_RGB|8,
		IMGFMT_RGB|15,
		IMGFMT_RGB|16,
		IMGFMT_RGB|24,
		IMGFMT_RGB|32,
		IMGFMT_BGR|8,
		IMGFMT_BGR|15,
		IMGFMT_BGR|16,
		IMGFMT_BGR|24,
		IMGFMT_BGR|32
	};
	static char *flagstr[] = {
		"flip",
		"noflip",
		"yuvhack",
		NULL
	};

	int i, j, freeslots;
	unsigned char flags;

	for (i = 0; i < CODECS_MAX_OUTFMT && outfmt[i] != 0xffffffff; i++)
		/* NOTHING */;
	freeslots = CODECS_MAX_OUTFMT - i;
	if (!freeslots)
		goto err_out_too_many;

	flags = 0;
	if(sflags) do {
		for (j = 0; flagstr[j] != NULL; j++)
			if (!strncmp(sflags, flagstr[j], strlen(flagstr[j])))
				break;
		if (flagstr[j] == NULL) return 0; // error!
		flags|=(1<<j);
		sflags+=strlen(flagstr[j]);
	} while (*(sflags++) == ',');

	do {
		for (j = 0; fmtstr[j] != NULL; j++)
			if (!strncmp(sfmt, fmtstr[j], strlen(fmtstr[j])))
				break;
		if (fmtstr[j] == NULL)
			return 0;
		outfmt[i] = fmtnum[j];
		outflags[i] = flags;
                ++i;
		sfmt+=strlen(fmtstr[j]);
	} while ((*(sfmt++) == ',') && --freeslots);

	if (!freeslots)
		goto err_out_too_many;

	if (*(--sfmt) != '\0') return 0;
        
	return 1;
err_out_too_many:
	printf("\ntoo many out...\n");
	return 0;
}

static short get_driver(char *s,int audioflag)
{
	static char *audiodrv[] = {
		"mp3lib",
		"pcm",
		"libac3",
		"acm",
		"alaw",
		"msgsm",
		"dshow",
		NULL
	};
	static char *videodrv[] = {
		"libmpeg2",
		"vfw",
		"odivx",
		"dshow",
		NULL
	};
        char **drv=audioflag?audiodrv:videodrv;
        int i;
        
        for(i=0;drv[i];i++) if(!strcmp(s,drv[i])) return i+1;

	return 0;
}

static int validate_codec(codecs_t *c, int type)
{
	int i;

	for (i = 0; i < strlen(c->name) && isalnum(c->name[i]); i++)
		/* NOTHING */;
	if (i < strlen(c->name)) {
		printf("\ncodec(%s)->name is not valid!\n", c->name);
		return 0;
	}
#warning codec->info = codec->name; ez ok, vagy strdup()?
	if (!c->info)
		c->info = c->name;
	if (c->fourcc[0] == 0xffffffff) {
		printf("\ncodec(%s) does not have fourcc/format!\n", c->name);
		return 0;
	}
	if (!c->driver) {
		printf("\ncodec(%s) does not have a driver!\n", c->name);
		return 0;
	}
#warning codec->driver == 4;... <- ezt nem kellene belehegeszteni...
#warning HOL VANNAK DEFINIALVA????????????
	if (!c->dll && (c->driver == 4 ||
				(c->driver == 2 && type == TYPE_VIDEO))) {
		printf("\ncodec(%s) needs a 'dll'!\n", c->name);
		return 0;
	}
#warning guid.f1 lehet 0? honnan lehet tudni, hogy nem adtak meg?
//	if (!(codec->flags & CODECS_FLAG_AUDIO) && codec->driver == 4)

	if (type == TYPE_VIDEO)
		if (c->outfmt[0] == 0xffffffff) {
			printf("\ncodec(%s) needs an 'outfmt'!\n", c->name);
			return 0;
		}
	return 1;
}

static int add_comment(char *s, char **d)
{
	int pos;

	if (!*d)
		pos = 0;
	else {
		pos = strlen(*d);
		(*d)[pos++] = '\n';
	}
	if (!(*d = (char *) realloc(*d, pos + strlen(s) + 1))) {
		printf("can't allocate mem for comment\n");
		return 0;
	}
	strcpy(*d + pos, s);
	return 1;
}

static FILE *fp;
static int line_num = 0;
static char *line;
static char *token[MAX_NR_TOKEN];

static int get_token(int min, int max)
{
	static int read_nextline = 1;
	static int line_pos;
	int i;
	char c;

	if (max >= MAX_NR_TOKEN) {
		printf("\nget_token(): max >= MAX_NR_TOKEN!\n");
		goto out_eof;
	}

	memset(token, 0x00, sizeof(*token) * max);

	if (read_nextline) {
		if (!fgets(line, MAX_LINE_LEN, fp))
			goto out_eof;
		line_pos = 0;
		++line_num;
		read_nextline = 0;
	}
	for (i = 0; i < max; i++) {
		while (isspace(line[line_pos]))
			++line_pos;
		if (line[line_pos] == '\0' || line[line_pos] == '#' ||
				line[line_pos] == ';') {
			read_nextline = 1;
			if (i >= min)
				goto out_ok;
			goto out_eol;
		}
		token[i] = line + line_pos;
		c = line[line_pos];
		if (c == '"' || c == '\'') {
			token[i]++;
			while (line[++line_pos] != c && line[line_pos])
				/* NOTHING */;
		} else {
			for (/* NOTHING */; !isspace(line[line_pos]) &&
					line[line_pos]; line_pos++)
				/* NOTHING */;
		}
		if (!line[line_pos]) {
			read_nextline = 1;
			if (i >= min - 1)
				goto out_ok;
			goto out_eol;
		}
		line[line_pos] = '\0';
		line_pos++;
	}
out_ok:
	return i;
out_eof:
	return RET_EOF;
out_eol:
	return RET_EOL;
}

static codecs_t *video_codecs=NULL;
static codecs_t *audio_codecs=NULL;
static int nr_vcodecs = 0;
static int nr_acodecs = 0;

codecs_t **parse_codec_cfg(char *cfgfile)
{
	codecs_t *codec = NULL; // current codec
	codecs_t **codecsp = NULL;// points to audio_codecs or to video_codecs
	static codecs_t *ret_codecs[2] = {NULL,NULL};
	int *nr_codecsp;
	int codec_type;		/* TYPE_VIDEO/TYPE_AUDIO */
	int tmp, i;

#ifdef DEBUG
	assert(cfgfile != NULL);
#endif

	printf("Reading codec config file: %s\n", cfgfile);

	if ((fp = fopen(cfgfile, "r")) == NULL) {
		printf("can't open '%s': %s\n", cfgfile, strerror(errno));
		return NULL;
	}

	if ((line = (char *) malloc(MAX_LINE_LEN + 1)) == NULL) {
		perror("can't get memory for 'line'");
		return NULL;
	}

	/*
	 * check if the cfgfile starts with 'audiocodec' or
	 * with 'videocodec'
	 */
	while ((tmp = get_token(1, 1)) == RET_EOL)
		/* NOTHING */;
	if (tmp != RET_EOF && (!strcmp(token[0], "audiocodec") ||
			!strcmp(token[0], "videocodec")))
		goto loop_enter;
	goto out;

	while ((tmp = get_token(1, 1)) != RET_EOF) {
		if (tmp == RET_EOL)
			continue;
		if (!strcmp(token[0], "audiocodec") ||
				!strcmp(token[0], "videocodec")) {
			if (!validate_codec(codec, codec_type))
				goto err_out_not_valid;
		loop_enter:
			if (*token[0] == 'v') {
				codec_type = TYPE_VIDEO;
				nr_codecsp = &nr_vcodecs;
				codecsp = &video_codecs;
			} else if (*token[0] == 'a') {
				codec_type = TYPE_AUDIO;
				nr_codecsp = &nr_acodecs;
				codecsp = &audio_codecs;
			} else {
				printf("rohattkurva\n");
				goto err_out;
			}
		        if (!(*codecsp = (codecs_t *) realloc(*codecsp,
				sizeof(codecs_t) * (*nr_codecsp + 1)))) {
			    perror("can't realloc '*codecsp'");
			    goto err_out;
		        }
			codec=*codecsp + *nr_codecsp;
			++*nr_codecsp;
                        memset(codec,0,sizeof(codecs_t));
			memset(codec->fourcc, 0xff, sizeof(codec->fourcc));
			memset(codec->outfmt, 0xff, sizeof(codec->outfmt));
                        
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			for (i = 0; i < *nr_codecsp - 1; i++) {
#warning audio meg videocodecnek lehet ugyanaz a neve? (most lehet...)
				if (!strcmp(token[0], (*codecsp)[i].name)) {
					PRINT_LINENUM;
					printf("codec name '%s' isn't unique\n", token[0]);
					goto err_out;
				}
			}
			if (!(codec->name = strdup(token[0]))) {
				perror("can't strdup -> 'name'");
				goto err_out;
			}
		} else if (!strcmp(token[0], "info")) {
			if (codec->info || get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!(codec->info = strdup(token[0]))) {
				perror("can't strdup -> 'info'");
				goto err_out;
			}
		} else if (!strcmp(token[0], "comment")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!add_comment(token[0], &codec->comment)) {
				PRINT_LINENUM;
				printf("add_comment()-tel valami sux\n");
			}
		} else if (!strcmp(token[0], "fourcc")) {
			if (get_token(1, 2) < 0)
				goto err_out_parse_error;
			if (!add_to_fourcc(token[0], token[1],
						codec->fourcc,
						codec->fourccmap))
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "format")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!add_to_format(token[0], codec->fourcc,codec->fourccmap))
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "driver")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if ((codec->driver = get_driver(token[0],codec_type)) == -1)
				goto err_out;
		} else if (!strcmp(token[0], "dll")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!(codec->dll = strdup(token[0]))) {
				perror("can't strdup -> 'dll'");
				goto err_out;
			}
		} else if (!strcmp(token[0], "guid")) {
			if (get_token(11, 11) < 0)
				goto err_out_parse_error;
#warning GUID-nak szammal kell kezdodni!!!!!!!! ez igy ok?
			for (i = 0; i < 11; i++)
				if (!isdigit(*token[i]))
					goto err_out_parse_error;
                        codec->guid.f1=strtoul(token[0],NULL,0);
                        codec->guid.f2=strtoul(token[1],NULL,0);
                        codec->guid.f3=strtoul(token[2],NULL,0);
			for (i = 0; i < 8; i++) {
                            codec->guid.f4[i]=strtoul(token[i + 3],NULL,0);
			}
		} else if (!strcmp(token[0], "out")) {
			if (get_token(1, 2) < 0)
				goto err_out_parse_error;
			if (!add_to_out(token[0], token[1], codec->outfmt,
						codec->outflags))
				goto err_out;
		} else if (!strcmp(token[0], "flags")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!strcmp(token[0], "seekable"))
				codec->flags |= CODECS_FLAG_SEEKABLE;
			else
				goto err_out_parse_error;
		} else if (!strcmp(token[0], "status")) {
			if (get_token(1, 1) < 0)
				goto err_out_parse_error;
			if (!strcasecmp(token[0], ":-)"))
				codec->status = CODECS_STATUS_WORKING;
			else if (!strcasecmp(token[0], ":-("))
				codec->status = CODECS_STATUS_NOT_WORKING;
			else if (!strcasecmp(token[0], "X-("))
				codec->status = CODECS_STATUS_UNTESTED;
			else if (!strcasecmp(token[0], ":-|"))
				codec->status = CODECS_STATUS_PROBLEMS;
			else
				goto err_out_parse_error;
		} else
			goto err_out_parse_error;
	}
	if (!validate_codec(codec, codec_type))
		goto err_out_not_valid;
	ret_codecs[0] = video_codecs;
	ret_codecs[1] = audio_codecs;
out:
	free(line);
	fclose(fp);
	return ret_codecs;
err_out_parse_error:
	PRINT_LINENUM;
	printf("parse error\n");
err_out:
	printf("\nOops\n");
	if (audio_codecs)
		free(audio_codecs);
	if (video_codecs)
		free(video_codecs);
	free(line);
	free(fp);
	return NULL;
err_out_not_valid:
	PRINT_LINENUM;
	printf("codec is not definied correctly\n");
	goto err_out;
}

codecs_t *find_audio_codec(unsigned int fourcc, unsigned int *fourccmap)
{
	return find_codec(fourcc, fourccmap, 1);
}

codecs_t *find_video_codec(unsigned int fourcc, unsigned int *fourccmap)
{
	return find_codec(fourcc, fourccmap, 0);
}

codecs_t* find_codec(unsigned int fourcc,unsigned int *fourccmap,int audioflag)
{
	int i, j;
	codecs_t *c;

	if (audioflag) {
		i = nr_acodecs;
		c = audio_codecs;
	} else {
		i = nr_vcodecs;
		c = video_codecs;
	}
	for (/* NOTHING */; i--; c++) {
		for (j = 0; j < CODECS_MAX_FOURCC; j++) {
			if (c->fourcc[j] == fourcc) {
				if (fourccmap) *fourccmap = c->fourccmap[j];
				return c;
			}
		}
	}
	return NULL;
}


#ifdef TESTING
int main(void)
{
	codecs_t **codecs, *c;
        int i,j, nr_codecs, state;

	if (!(codecs = parse_codec_cfg("DOCS/codecs.conf")))
		return 0;
	if (!codecs[0])
		printf("no videoconfig.\n");
	if (!codecs[1])
		printf("no audioconfig.\n");

	printf("videocodecs:\n");
	c = codecs[0];
	nr_codecs = nr_vcodecs;
	state = 0;
next:
	if (c) {
		printf("number of codecs: %d\n", nr_codecs);
		for(i=0;i<nr_codecs;i++, c++){
		    printf("\n============== codec %02d ===============\n",i);
		    printf("name='%s'\n",c->name);
		    printf("info='%s'\n",c->info);
		    printf("comment='%s'\n",c->comment);
		    printf("dll='%s'\n",c->dll);
		    printf("flags=%X  driver=%d\n",c->flags,c->driver);

		    for(j=0;j<CODECS_MAX_FOURCC;j++){
		      if(c->fourcc[j]!=0xFFFFFFFF){
			  printf("fourcc %02d:  %08X (%.4s) ===> %08X (%.4s)\n",j,c->fourcc[j],&c->fourcc[j],c->fourccmap[j],&c->fourccmap[j]);
		      }
		    }

		    for(j=0;j<CODECS_MAX_OUTFMT;j++){
		      if(c->outfmt[j]!=0xFFFFFFFF){
			  printf("outfmt %02d:  %08X (%.4s)  flags: %d\n",j,c->outfmt[j],&c->outfmt[j],c->outflags[j]);
		      }
		    }

		    printf("GUID: %08lX %04X %04X",c->guid.f1,c->guid.f2,c->guid.f3);
		    for(j=0;j<8;j++) printf(" %02X",c->guid.f4[j]);
		    printf("\n");

		    
		}
	}
	if (!state) {
		printf("audiocodecs:\n");
		c = codecs[1];
		nr_codecs = nr_acodecs;
		state = 1;
		goto next;
	}
	return 0;
}

#endif

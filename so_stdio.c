#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>

#include "so_stdio.h"

#define BUFF    4096

struct _so_file
{
int fd;
char *buff;
int crt_pos;
int crt_bufsize;
long bytes;
int flag_error;
int last_op;
int pid;
};

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int file_des = -1;

	switch (*mode) {
	case 'r':
		if (*(mode + 1)) {
			if (strchr(mode + 1, '+'))
				file_des = open(pathname, O_RDWR, 0644);
		} else
			file_des = open(pathname, O_RDONLY, 0644);
		break;
	case 'w':
		if (*(mode + 1)) {
			if (strchr(mode + 1, '+'))
				file_des = open(pathname,
					O_RDWR | O_CREAT | O_TRUNC, 0644);
		} else
			file_des = open(pathname,
				 O_WRONLY | O_CREAT | O_TRUNC, 0644);
		break;
	case 'a':
		if (*(mode + 1)) {
			if (strchr(mode + 1, '+'))
				file_des = open(pathname,
				 O_RDWR | O_CREAT | O_APPEND, 0644);
		} else
			file_des = open(pathname,
				 O_WRONLY | O_CREAT | O_APPEND, 0644);
		break;
	}

	if (file_des < 0)
		return NULL;

	SO_FILE *fl = (SO_FILE *) calloc(1, sizeof(SO_FILE));

	fl->buff = (char *) calloc(BUFF, sizeof(char));

	memset(fl->buff, 0, BUFF);

	fl->crt_pos = fl->last_op = fl->bytes = fl->flag_error = 0;
	fl->fd = file_des;
	fl->crt_bufsize = BUFF;

	return fl;
}

int so_fclose(SO_FILE *stream)
{
	int ret;

	if (so_fflush(stream) != EOF) {
		ret = close(stream->fd);
		if (ret != -1) {
			free(stream->buff);
			free(stream);
			return ret;
		}
	}
	free(stream->buff);
	free(stream);
	return EOF;
}


int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_fflush(SO_FILE *stream)
{
	if (stream->crt_pos == 0)
		return 0;

	else if (stream->crt_pos != 0) {
		if (stream->last_op == 1 &&
		write(stream->fd, stream->buff, stream->crt_pos) > -1) {
			if (stream != NULL) {
				memset(stream->buff, 0, BUFF);
				stream->crt_pos = 0;
				stream->crt_bufsize = BUFF;

				return 0;
			}
		} else if (stream->last_op == 2) {
			if (stream != NULL) {
				memset(stream->buff, 0, BUFF);
				stream->crt_pos = 0;
				stream->crt_bufsize = BUFF;

			return 0;
			}
		} else {
			stream->flag_error = 1;
			return EOF;
		}
	} else
		stream->flag_error = 1;

	return EOF;
}


int so_fseek(SO_FILE *stream, long offset, int whence)
{
	so_fflush(stream);
	int seeked = lseek(stream->fd, offset, whence);

	int ret = -1;

	if (seeked >= 0) {
		stream->bytes = seeked;
		ret = 0;
	}

	return ret;
}

long so_ftell(SO_FILE *stream)
{
	long ret = stream->crt_pos + stream->bytes;

	return ret;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	for (int i = 0; i < nmemb * size; ++i) {
		int charGet = so_fgetc(stream);

		if (so_feof(stream) == 0)
			*((char *)(ptr + i)) = charGet;
		else
			return i / size;
	}
	return nmemb;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	if (nmemb * size == 0)
		return 0;

	for (int i = 0; i < nmemb * size; ++i) {
		if (so_fputc(*((char *)ptr + i), stream) == EOF &&
		*((char *)ptr + i) != EOF)
			return i / size;
	}
	return nmemb;
}

int so_fgetc(SO_FILE *stream)
{
	stream->last_op = 2;
	int bts = 0;

	if ((stream->crt_pos == stream->crt_bufsize) ||
	(stream->crt_pos == 0 && stream->buff[stream->crt_pos] == '\0')) {
		bts = read(stream->fd, stream->buff, BUFF);
		if (bts <= 0) {
			stream->flag_error = 1;
			return EOF;
		}
		if (stream->crt_pos == stream->crt_bufsize && bts > 0) {
			stream->bytes += bts;
			stream->crt_bufsize = bts;
			stream->crt_pos = 0;
		} else if (bts > 0) {
			stream->crt_bufsize = bts;
			stream->crt_pos = 0;
		}
	}
	stream->crt_pos++;
	return (int)stream->buff[stream->crt_pos - 1];
}

int so_fputc(int c, SO_FILE *stream)
{
	stream->last_op = 1;
	if (stream->crt_bufsize <= stream->crt_pos) {
		if (stream != NULL) {
			stream->bytes += stream->crt_bufsize;

			if (so_fflush(stream) == EOF)
				return so_fflush(stream);
		}
	}
	if (stream != NULL) {
		stream->buff[stream->crt_pos] = c;
		stream->crt_pos++;
		return c;
	}
	stream->flag_error = 1;
	return EOF;
}

int so_feof(SO_FILE *stream)
{
	int ret = so_ferror(stream);

	return ret;
}

int so_ferror(SO_FILE *stream)
{
	int ret = stream->flag_error;

	return ret;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	int fd[2];
	int child = -1;

	SO_FILE *fl = (SO_FILE *) calloc(1, sizeof(SO_FILE));

	fl->buff = (char *) calloc(BUFF, sizeof(char));

	pipe(fd);
	child = fork();

	if (child == 0 && strncmp(type, "r", sizeof(char)) == 0) {
		close(fd[0]);
		if (fd[1] != STDOUT_FILENO) {
			dup2(fd[1], STDOUT_FILENO);
			close(fd[1]);
			fd[1] = STDOUT_FILENO;
		}
	} else if (child == 0 && strncmp(type, "w", sizeof(char)) == 0) {
		close(fd[1]);
		if (fd[0] != STDIN_FILENO) {
			dup2(fd[0], STDIN_FILENO);
			close(fd[0]);
		}
	}
	if (child == 0) {
		execlp("/bin/sh", "sh", "-c", command, NULL);
		return NULL;
	} else if (child < 0) {
		free(fl->buff);
		free(fl);
		return NULL;
	}

	int file_des;

	if (strncmp(type, "r", sizeof(char)) == 0) {
		close(fd[1]);
		file_des = fd[0];
	}

	if (strncmp(type, "w", sizeof(char)) == 0) {
		close(fd[0]);
		file_des = fd[1];
	}

	memset(fl->buff, 0, BUFF);
	fl->fd = file_des;
	fl->crt_pos = fl->last_op = fl->flag_error = fl->bytes = 0;
	fl->pid = child;
	fl->crt_bufsize = BUFF;

	return fl;
}

int so_pclose(SO_FILE *stream)
{
	int expectedPid = stream->pid;

	so_fclose(stream);

	int pstat = -1;

	if (waitpid(expectedPid, &pstat, 0) == -1)
		return waitpid(expectedPid, &pstat, 0);

	return pstat;
}

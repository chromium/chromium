
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
/*-
 * Copyright 2003-2005 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if 0
__FBSDID("$FreeBSD: src/usr.bin/bsdiff/bsdiff/bsdiff.c,v 1.1 2005/08/06 01:59:05 cperciva Exp $");
#endif

#include <sys/types.h>

#include <bzlib.h>
#include <err.h>
#include <fcntl.h>
#include <lzma.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#include <libkern/OSByteOrder.h>
#define htole64(x) OSSwapHostToLittleInt64(x)
#elif defined(__linux__)
#include <endian.h>
#elif defined(_WIN32) && (defined(_M_IX86) || defined(_M_X64))
#define htole64(x) (x)
#else
#error Provide htole64 for this platform
#endif

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static void split(off_t *I,off_t *V,off_t start,off_t len,off_t h)
{
	off_t i,j,k,x,tmp,jj,kk;

	if(len<16) {
		for(k=start;k<start+len;k+=j) {
			j=1;x=V[I[k]+h];
			for(i=1;k+i<start+len;i++) {
				if(V[I[k+i]+h]<x) {
					x=V[I[k+i]+h];
					j=0;
				};
				if(V[I[k+i]+h]==x) {
					tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;
					j++;
				};
			};
			for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
			if(j==1) I[k]=-1;
		};
		return;
	};

	x=V[I[start+len/2]+h];
	jj=0;kk=0;
	for(i=start;i<start+len;i++) {
		if(V[I[i]+h]<x) jj++;
		if(V[I[i]+h]==x) kk++;
	};
	jj+=start;kk+=jj;

	i=start;j=0;k=0;
	while(i<jj) {
		if(V[I[i]+h]<x) {
			i++;
		} else if(V[I[i]+h]==x) {
			tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;
			j++;
		} else {
			tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	while(jj+j<kk) {
		if(V[I[jj+j]+h]==x) {
			j++;
		} else {
			tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	if(jj>start) split(I,V,start,jj-start,h);

	for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
	if(jj==kk-1) I[jj]=-1;

	if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

static void qsufsort(off_t *I,off_t *V,u_char *old,off_t oldsize)
{
	off_t buckets[256];
	off_t i,h,len;

	for(i=0;i<256;i++) buckets[i]=0;
	for(i=0;i<oldsize;i++) buckets[old[i]]++;
	for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
	for(i=255;i>0;i--) buckets[i]=buckets[i-1];
	buckets[0]=0;

	for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
	I[0]=oldsize;
	for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
	V[oldsize]=0;
	for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
	I[0]=-1;

	for(h=1;I[0]!=-(oldsize+1);h+=h) {
		len=0;
		for(i=0;i<oldsize+1;) {
			if(I[i]<0) {
				len-=I[i];
				i-=I[i];
			} else {
				if(len) I[i-len]=-len;
				len=V[I[i]]+1-i;
				split(I,V,i,len,h);
				i+=len;
				len=0;
			};
		};
		if(len) I[i-len]=-len;
	};

	for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

static off_t matchlen(u_char *old,off_t oldsize,u_char *new,off_t newsize)
{
	off_t i;

	for(i=0;(i<oldsize)&&(i<newsize);i++)
		if(old[i]!=new[i]) break;

	return i;
}

static off_t search(off_t *I,u_char *old,off_t oldsize,
		u_char *new,off_t newsize,off_t st,off_t en,off_t *pos)
{
	off_t x,y;

	if(en-st<2) {
		x=matchlen(old+I[st],oldsize-I[st],new,newsize);
		y=matchlen(old+I[en],oldsize-I[en],new,newsize);

		if(x>y) {
			*pos=I[st];
			return x;
		} else {
			*pos=I[en];
			return y;
		}
	};

	x=st+(en-st)/2;
	if(memcmp(old+I[x],new,MIN(oldsize-I[x],newsize))<0) {
		return search(I,old,oldsize,new,newsize,x,en,pos);
	} else {
		return search(I,old,oldsize,new,newsize,st,x,pos);
	};
}

static inline void offtout(off_t x,u_char *buf)
{
	*((off_t*)buf) = htole64(x);
}

/* zlib provides compress2, which deflates to deflate (zlib) format. This is
 * unfortunately distinct from gzip format in that the headers wrapping the
 * decompressed data are different. gbspatch reads gzip-compressed data using
 * the file-oriented gzread interface, which only supports gzip format.
 * compress2gzip is identical to zlib's compress2 except that it produces gzip
 * output compatible with gzread. This change is achieved by calling
 * deflateInit2 instead of deflateInit and specifying 31 for windowBits;
 * numbers greater than 15 cause the addition of a gzip wrapper. */
static int compress2gzip(Bytef *dest, uLongf *destLen,
                         const Bytef *source, uLong sourceLen, int level)
{
	z_stream stream;
	int err;

	stream.next_in = (Bytef*)source;
	stream.avail_in = (uInt)sourceLen;

	stream.next_out = dest;
	stream.avail_out = (uInt)*destLen;
	if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;

	err = deflateInit2(&stream,
	                   level, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
	if (err != Z_OK) return err;

	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&stream);
		return err == Z_OK ? Z_BUF_ERROR : err;
	}
	*destLen = stream.total_out;

	err = deflateEnd(&stream);
	return err;
}

/* Recompress buf of size buf_len using bzip2 or gzip. The smallest version is
 * used. The original uncompressed variant may be the smallest. Returns a
 * number identifying the encoding, 1 for uncompressed, 2 for bzip2, 3 for
 * gzip, and 4 for xz/lzma2. If the original uncompressed variant is not
 * smallest, it is freed. The caller must free any buf after this function
 * returns. */
static char make_small(u_char **buf, off_t *buf_len)
{
	u_char *source = *buf;
	off_t source_len = *buf_len;
	u_char *bz2, *gz, *lzma;
	unsigned int bz2_len;
	size_t gz_len, lzma_len, lzma_pos;
	int bz2_err, gz_err;
	lzma_ret lzma_err;
	lzma_check lzma_ck;
	char smallest;

	smallest = 1;

	bz2_len = source_len + 1;
	bz2 = malloc(bz2_len);
	bz2_err = BZ2_bzBuffToBuffCompress((char*)bz2, &bz2_len, (char*)source,
	                                   source_len, 9, 0, 0);
	if (bz2_err == BZ_OK) {
		if (bz2_len < *buf_len) {
			smallest = 2;
			*buf = bz2;
			*buf_len = bz2_len;
		} else {
			free(bz2);
			bz2 = NULL;
		}
	} else if (bz2_err == BZ_OUTBUFF_FULL) {
		free(bz2);
		bz2 = NULL;
	} else {
		errx(1, "BZ2_bzBuffToBuffCompress: %d", bz2_err);
	}

	gz_len = source_len + 1;
	gz = malloc(gz_len);
	gz_err = compress2gzip(gz, &gz_len, source, source_len, 9);
	if (gz_err == Z_OK) {
		if (gz_len < *buf_len) {
			smallest = 3;
			*buf = gz;
			*buf_len = gz_len;
		} else {
			free(gz);
			gz = NULL;
		}
	} else if (gz_err == Z_BUF_ERROR) {
		free(gz);
		gz = NULL;
	} else {
		errx(1, "compress2gzip: %d", gz_err);
	}

	lzma_len = source_len + 1;
	lzma = malloc(lzma_len);
	lzma_pos = 0;

	/* Equivalent to the options used by xz -9 -e. */
	lzma_ck = LZMA_CHECK_CRC64;
	if (!lzma_check_is_supported(lzma_ck))
		lzma_ck = LZMA_CHECK_CRC32;
	lzma_err = lzma_easy_buffer_encode(9 | LZMA_PRESET_EXTREME,
	                                   lzma_ck, NULL,
	                                   source, source_len,
	                                   lzma, &lzma_pos, lzma_len);
	if (lzma_err == LZMA_OK) {
		if (lzma_pos < *buf_len) {
			smallest = 4;
			*buf = lzma;
			*buf_len = lzma_pos;
		} else {
			free(lzma);
			lzma = NULL;
		}
	} else if (lzma_err == LZMA_BUF_ERROR) {
		free(lzma);
		lzma = NULL;
	} else {
		errx(1, "lzma_easy_buffer_encode: %d", lzma_err);
	}

	if (smallest != 1) {
		free(source);
	}

	return smallest;
}

int main(int argc,char *argv[])
{
	int fd;
	u_char *old,*new;
	off_t oldsize,newsize;
	off_t *I,*V;
	off_t scan,pos,len;
	off_t lastscan,lastpos,lastoffset;
	off_t oldscore,scsc;
	off_t s,Sf,lenf,Sb,lenb;
	off_t overlap,Ss,lens;
	off_t i;
	off_t cblen, dblen, eblen;
	u_char *cb, *db, *eb;
	u_char header[96];
	FILE * pf;

	if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile",argv[0]);

	/* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if(((fd=open(argv[1],O_RDONLY,0))<0) ||
		((oldsize=lseek(fd,0,SEEK_END))==-1) ||
		((old=malloc(oldsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,old,oldsize)!=oldsize) ||
		(close(fd)==-1)) err(1,"%s",argv[1]);

	if(((I=malloc((oldsize+1)*sizeof(off_t)))==NULL) ||
		((V=malloc((oldsize+1)*sizeof(off_t)))==NULL)) err(1,NULL);

	qsufsort(I,V,old,oldsize);

	free(V);

	/* Allocate newsize+1 bytes instead of newsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if(((fd=open(argv[2],O_RDONLY,0))<0) ||
		((newsize=lseek(fd,0,SEEK_END))==-1) ||
		((new=malloc(newsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,new,newsize)!=newsize) ||
		(close(fd)==-1)) err(1,"%s",argv[2]);

	if(((cb=malloc(newsize+1))==NULL) ||
		((db=malloc(newsize+1))==NULL) ||
		((eb=malloc(newsize+1))==NULL)) err(1,NULL);
	cblen=0;
	dblen=0;
	eblen=0;

	/* Create the patch file */
	if ((pf = fopen(argv[3], "wb")) == NULL)
		err(1, "%s", argv[3]);

	/* File format:
		0	8	"BSDIFF4G"
		8	8	length of compressed control block (x)
		16	8	length of compressed diff block (y)
		24	8	length of compressed extra block (z)
		32	8	length of old file
		40	8	length of new file
		48	20	SHA1 of old file
		68	20	SHA1 of new file
		88	1	encoding of control block
		89	1	encoding of diff block
		90	1	encoding of extra block
		91	5	unused
		96	x	compressed control block
		96+x	y	compressed diff block
		96+x+y	z	compressed extra block
	Encodings are 1 (uncompressed), 2 (bzip2), 3 (gzip), and 4 (xz/lzma2).
	*/

	memset(header, 0, sizeof(header));
	if (fwrite(header, sizeof(header), 1, pf) != 1)
		err(1, "fwrite(%s)", argv[3]);
	memcpy(header, "BSDIFF4G", 8);
	offtout(oldsize, header + 32);
	offtout(newsize, header + 40);
	CC_SHA1(old, oldsize, header + 48);
	CC_SHA1(new, newsize, header + 68);

	/* Compute the differences */
	scan=0;len=0;
	lastscan=0;lastpos=0;lastoffset=0;
	while(scan<newsize) {
		oldscore=0;

		for(scsc=scan+=len;scan<newsize;scan++) {
			len=search(I,old,oldsize,new+scan,newsize-scan,
					0,oldsize,&pos);

			for(;scsc<scan+len;scsc++)
			if((scsc+lastoffset<oldsize) &&
				(old[scsc+lastoffset] == new[scsc]))
				oldscore++;

			if(((len==oldscore) && (len!=0)) || 
				(len>oldscore+8)) break;

			if((scan+lastoffset<oldsize) &&
				(old[scan+lastoffset] == new[scan]))
				oldscore--;
		};

		if((len!=oldscore) || (scan==newsize)) {
			s=0;Sf=0;lenf=0;
			for(i=0;(lastscan+i<scan)&&(lastpos+i<oldsize);) {
				if(old[lastpos+i]==new[lastscan+i]) s++;
				i++;
				if(s*2-i>Sf*2-lenf) { Sf=s; lenf=i; };
			};

			lenb=0;
			if(scan<newsize) {
				s=0;Sb=0;
				for(i=1;(scan>=lastscan+i)&&(pos>=i);i++) {
					if(old[pos-i]==new[scan-i]) s++;
					if(s*2-i>Sb*2-lenb) { Sb=s; lenb=i; };
				};
			};

			if(lastscan+lenf>scan-lenb) {
				overlap=(lastscan+lenf)-(scan-lenb);
				s=0;Ss=0;lens=0;
				for(i=0;i<overlap;i++) {
					if(new[lastscan+lenf-overlap+i]==
					   old[lastpos+lenf-overlap+i]) s++;
					if(new[scan-lenb+i]==
					   old[pos-lenb+i]) s--;
					if(s>Ss) { Ss=s; lens=i+1; };
				};

				lenf+=lens-overlap;
				lenb-=lens;
			};

			for(i=0;i<lenf;i++)
				db[dblen+i]=new[lastscan+i]-old[lastpos+i];
			for(i=0;i<(scan-lenb)-(lastscan+lenf);i++)
				eb[eblen+i]=new[lastscan+lenf+i];

			dblen+=lenf;
			eblen+=(scan-lenb)-(lastscan+lenf);

			offtout(lenf, cb + cblen);
			cblen += 8;

			offtout((scan - lenb) - (lastscan + lenf), cb + cblen);
			cblen += 8;

			offtout((pos - lenb) - (lastpos + lenf), cb + cblen);
			cblen += 8;

			lastscan=scan-lenb;
			lastpos=pos-lenb;
			lastoffset=pos-scan;
		};
	};

	header[88] = make_small(&cb, &cblen);
	header[89] = make_small(&db, &dblen);
	header[90] = make_small(&eb, &eblen);

	if (fwrite(cb, 1, cblen, pf) != cblen)
		err(1, "fwrite");
	if (fwrite(db, 1, dblen, pf) != dblen)
		err(1, "fwrite");
	if (fwrite(eb, 1, eblen, pf) != eblen)
		err(1, "fwrite");

	offtout(cblen, header + 8);
	offtout(dblen, header + 16);
	offtout(eblen, header + 24);

	/* Seek to the beginning, write the header, and close the file */
	if (fseeko(pf, 0, SEEK_SET))
		err(1, "fseeko");
	if (fwrite(header, sizeof(header), 1, pf) != 1)
		err(1, "fwrite(%s)", argv[3]);
	if (fclose(pf))
		err(1, "fclose");

	/* Free the memory we used */
	free(cb);
	free(db);
	free(eb);
	free(I);
	free(old);
	free(new);

	return 0;
}

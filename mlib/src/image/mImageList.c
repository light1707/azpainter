/*$
 Copyright (C) 2013-2020 Azel.

 This file is part of AzPainter.

 AzPainter is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 AzPainter is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
$*/

/*****************************************
 * mImageList
 *****************************************/

#include <string.h>

#include "mDef.h"
#include "mImageList.h"
#include "mLoadImage.h"
#include "mPixbuf.h"
#include "mPixbuf_pv.h"
#include "mGui.h"


/**************//**

@defgroup imagelist mImageList
@brief 横に複数個並んだイメージ

24bit イメージ (R-G-B 順)。

@ingroup group_image
@{

@file mImageList.h
@struct _mImageList

************/


/** mImageList 作成 */

static mImageList *_create_imagelist(int w,int h,int eachw,int32_t tpcol)
{
	mImageList *p;

	p = (mImageList *)mMalloc(sizeof(mImageList), TRUE);
	if(!p) return NULL;

	p->w = w;
	p->h = h;
	p->pitch = w * 3;
	p->eachw = (eachw > w)? w: eachw;
	p->tpcol = tpcol;
	p->num = w / p->eachw;

	//イメージバッファ

	p->buf = (uint8_t *)mMalloc(w * h * 3, FALSE);
	if(!p->buf)
	{
		mFree(p);
		return NULL;
	}

	return p;
}


/** 解放 */

void mImageListFree(mImageList *p)
{
	if(p)
	{
		if(p->buf) mFree(p->buf);

		mFree(p);
	}
}

/** mPixbuf に描画 */

void mImageListPutPixbuf(mImageList *p,mPixbuf *dst,int dx,int dy,int index,mBool disable)
{
	mPixbufBltInfo bi;
	uint8_t *pd,*ps;
	int ix,iy,pitchd,pitchs,r,g,b,dr,dg,db;

	if(!p || index < 0 || index >= p->num) return;

	if(!__mPixbufBltClip(&bi, dst, dx, dy, index * p->eachw, 0, p->eachw, p->h))
		return;

	//

	pd = mPixbufGetBufPtFast(dst, bi.dx, bi.dy);
	ps = p->buf + bi.sy * p->pitch + bi.sx * 3;

	pitchd = dst->pitch_dir - dst->bpp * bi.w;
	pitchs = p->pitch - bi.w * 3;

	//

	for(iy = bi.h; iy > 0; iy--)
	{
		for(ix = bi.w; ix > 0; ix--)
		{
			if(M_RGB(ps[0], ps[1], ps[2]) != p->tpcol)
			{
				r = ps[0];
				g = ps[1];
				b = ps[2];

				if(disable)
				{
					mPixbufGetPixelBufRGB(dst, pd, &dr, &dg, &db);

					r = g = b = (r + g + b) / 3;

					r = ((r - dr) >> 2) + dr;
					g = ((g - dg) >> 2) + dg;
					b = ((b - db) >> 2) + db;
				}
			
				(dst->setbuf)(pd, mRGBtoPix2(r, g, b));
			}
		
			pd += dst->bpp;
			ps += 3;
		}

		pd += pitchd;
		ps += pitchs;
	}
}


//=========================
// 読み込み
//=========================


typedef struct
{
	mLoadImage base;
	mImageList *img;
	uint8_t *dstbuf;
	int eachw;
	int32_t tpcol;
}LOADIMAGE_INFO;


/** 情報取得 */

static int _load_getinfo(mLoadImage *load,mLoadImageInfo *info)
{
	LOADIMAGE_INFO *p = (LOADIMAGE_INFO *)load;

	p->img = _create_imagelist(info->width, info->height, p->eachw, p->tpcol);
	if(!p->img) return MLOADIMAGE_ERR_ALLOC;

	p->dstbuf = p->img->buf;
	if(info->bottomup) p->dstbuf += (info->height - 1) * p->img->pitch;

	return MLOADIMAGE_ERR_OK;
}

/** Y1行取得 */

static int _load_getrow(mLoadImage *load,uint8_t *buf,int pitch)
{
	LOADIMAGE_INFO *p = (LOADIMAGE_INFO *)load;

	memcpy(p->dstbuf, buf, pitch);

	p->dstbuf += (load->info.bottomup)? -(p->img->pitch): p->img->pitch;

	return MLOADIMAGE_ERR_OK;
}

/** PNG から読み込み */

static mImageList *_load_png(const char *filename,const uint8_t *pngbuf,int bufsize,int eachw,int32_t tpcol)
{
	LOADIMAGE_INFO *p;
	mImageList *img;
	char *path = NULL;
	mBool ret;

	//mLoadImage

	p = (LOADIMAGE_INFO *)mLoadImage_create(sizeof(LOADIMAGE_INFO));
	if(!p) return NULL;

	if(pngbuf)
	{
		p->base.src.type = MLOADIMAGE_SOURCE_TYPE_BUF;
		p->base.src.buf = pngbuf;
		p->base.src.bufsize = bufsize;
	}
	else
	{
		path = mAppGetFilePath(filename);

		p->base.src.type = MLOADIMAGE_SOURCE_TYPE_PATH;
		p->base.src.filename = path;
	}

	p->base.format = MLOADIMAGE_FORMAT_RGB;
	p->base.getinfo = _load_getinfo;
	p->base.getrow = _load_getrow;

	p->eachw = eachw;
	p->tpcol = tpcol;

	//読み込み

	ret = mLoadImagePNG(M_LOADIMAGE(p));

	img = p->img;

	mLoadImage_free(M_LOADIMAGE(p));
	mFree(path);

	if(ret)
		return img;
	else
	{
		mImageListFree(img);
		return NULL;
	}
}


//=========


/** PNG ファイルから読み込み
 *
 * @param filename 「!/」で始まっている場合はデータディレクトリからの相対位置
 * @param tpcol    透過色(RGB)。負の値でなし。 */

mImageList *mImageListLoadPNG(const char *filename,int eachw,int32_t tpcol)
{
	return _load_png(filename, NULL, 0, eachw, tpcol);
}

/** PNG データをバッファから読み込み */

mImageList *mImageListLoadPNG_fromBuf(const uint8_t *buf,int bufsize,int eachw,int32_t tpcol)
{
	return _load_png(NULL, buf, bufsize, eachw, tpcol);
}

/** @} */

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Rect;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.media_session.MediaImage;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Robolectric tests for MediaImageManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaImageManagerTest {
    private static final int TINY_IMAGE_SIZE_PX = 50;
    private static final int MIN_IMAGE_SIZE_PX = 100;
    private static final int IDEAL_IMAGE_SIZE_PX = 200;
    private static final int REQUEST_ID_1 = 1;
    private static final int REQUEST_ID_2 = 2;
    private static final GURL IMAGE_URL_1 = JUnitTestGURLs.URL_1;
    private static final GURL IMAGE_URL_2 = JUnitTestGURLs.URL_2;

    @Mock private WebContents mWebContents;
    @Mock private MediaImageCallback mCallback;

    private MediaImageManager mMediaImageManager;

    // Prepared data for feeding.
    private ArrayList<MediaImage> mImages;
    private ArrayList<Bitmap> mBitmaps;
    private ArrayList<Rect> mOriginalImageSizes;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        MockitoAnnotations.initMocks(this);
        doReturn(REQUEST_ID_1)
                .when(mWebContents)
                .downloadImage(
                        any(), anyBoolean(), anyInt(), anyBoolean(), any(MediaImageManager.class));
        mMediaImageManager = new MediaImageManager(MIN_IMAGE_SIZE_PX, IDEAL_IMAGE_SIZE_PX);
        mMediaImageManager.setWebContents(mWebContents);

        mImages = new ArrayList<MediaImage>();
        mImages.add(new MediaImage(IMAGE_URL_1, "", new ArrayList<Rect>()));

        mBitmaps = new ArrayList<Bitmap>();
        mBitmaps.add(
                Bitmap.createBitmap(
                        IDEAL_IMAGE_SIZE_PX, IDEAL_IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888));

        mOriginalImageSizes = new ArrayList<Rect>();
        mOriginalImageSizes.add(new Rect(0, 0, IDEAL_IMAGE_SIZE_PX, IDEAL_IMAGE_SIZE_PX));
    }

    @Test
    public void testDownloadImage() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mWebContents)
                .downloadImage(
                        eq(IMAGE_URL_1),
                        eq(false),
                        eq(MediaImageManager.MAX_BITMAP_SIZE_FOR_DOWNLOAD),
                        eq(false),
                        eq(mMediaImageManager));
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        verify(mCallback).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDownloadSameImageTwice() {
        // First download.
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        // Second download.
        doReturn(REQUEST_ID_2)
                .when(mWebContents)
                .downloadImage(
                        any(), anyBoolean(), anyInt(), anyBoolean(), any(MediaImageManager.class));
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_2, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        verify(mWebContents, times(1))
                .downloadImage(
                        eq(IMAGE_URL_1),
                        eq(false),
                        eq(MediaImageManager.MAX_BITMAP_SIZE_FOR_DOWNLOAD),
                        eq(false),
                        eq(mMediaImageManager));
        verify(mCallback, times(1)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDownloadSameImageTwiceButFailed() {
        // First download.
        mBitmaps.clear();
        mOriginalImageSizes.clear();

        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 404, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        // Second download.
        mMediaImageManager.downloadImage(mImages, mCallback);
        // The second download request will never be initiated and the callback
        // will be ignored.
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        verify(mWebContents, times(1))
                .downloadImage(
                        eq(IMAGE_URL_1),
                        eq(false),
                        eq(MediaImageManager.MAX_BITMAP_SIZE_FOR_DOWNLOAD),
                        eq(false),
                        eq(mMediaImageManager));
        verify(mCallback, times(1)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDownloadDifferentImagesTwice() {
        // First download.
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        // Second download.
        doReturn(REQUEST_ID_2)
                .when(mWebContents)
                .downloadImage(
                        any(), anyBoolean(), anyInt(), anyBoolean(), any(MediaImageManager.class));
        mImages.clear();
        mImages.add(new MediaImage(IMAGE_URL_2, "", new ArrayList<Rect>()));

        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_2, 200, IMAGE_URL_2, mBitmaps, mOriginalImageSizes);

        verify(mWebContents, times(1))
                .downloadImage(
                        eq(IMAGE_URL_1),
                        eq(false),
                        eq(MediaImageManager.MAX_BITMAP_SIZE_FOR_DOWNLOAD),
                        eq(false),
                        eq(mMediaImageManager));
        verify(mWebContents, times(1))
                .downloadImage(
                        eq(IMAGE_URL_2),
                        eq(false),
                        eq(MediaImageManager.MAX_BITMAP_SIZE_FOR_DOWNLOAD),
                        eq(false),
                        eq(mMediaImageManager));
        verify(mCallback, times(2)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDownloadAnotherImageBeforeResponse() {
        // First download.
        mMediaImageManager.downloadImage(mImages, mCallback);

        // Second download.
        doReturn(REQUEST_ID_2)
                .when(mWebContents)
                .downloadImage(
                        any(), anyBoolean(), anyInt(), anyBoolean(), any(MediaImageManager.class));
        mImages.clear();
        mImages.add(new MediaImage(IMAGE_URL_2, "", new ArrayList<Rect>()));

        mMediaImageManager.downloadImage(mImages, mCallback);

        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_2, 200, IMAGE_URL_2, mBitmaps, mOriginalImageSizes);

        // This reply should not be sent to the client.
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        verify(mWebContents, times(1))
                .downloadImage(
                        eq(IMAGE_URL_1),
                        eq(false),
                        eq(MediaImageManager.MAX_BITMAP_SIZE_FOR_DOWNLOAD),
                        eq(false),
                        eq(mMediaImageManager));
        verify(mWebContents, times(1))
                .downloadImage(
                        eq(IMAGE_URL_2),
                        eq(false),
                        eq(MediaImageManager.MAX_BITMAP_SIZE_FOR_DOWNLOAD),
                        eq(false),
                        eq(mMediaImageManager));

        verify(mCallback, times(1)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDuplicateResponce() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        verify(mCallback, times(1)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testWrongResponceId() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_2, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testTinyImagesRemovedBeforeDownloading() {
        mImages.clear();
        ArrayList<Rect> sizes = new ArrayList<Rect>();
        sizes.add(new Rect(0, 0, TINY_IMAGE_SIZE_PX, TINY_IMAGE_SIZE_PX));
        mImages.add(new MediaImage(IMAGE_URL_1, "", sizes));
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mWebContents, times(0))
                .downloadImage(
                        any(), anyBoolean(), anyInt(), anyBoolean(), any(MediaImageManager.class));
        verify(mCallback).onImageDownloaded((Bitmap) isNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
    }

    @Test
    public void testTinyImagesRemovedAfterDownloading() {
        mMediaImageManager.downloadImage(mImages, mCallback);

        // Reset the data for feeding.
        mBitmaps.clear();
        mBitmaps.add(
                Bitmap.createBitmap(
                        TINY_IMAGE_SIZE_PX, TINY_IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888));
        mOriginalImageSizes.clear();
        mOriginalImageSizes.add(new Rect(0, 0, TINY_IMAGE_SIZE_PX, TINY_IMAGE_SIZE_PX));

        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 200, IMAGE_URL_1, mBitmaps, mOriginalImageSizes);

        verify(mCallback).onImageDownloaded((Bitmap) isNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
    }

    @Test
    public void testDownloadImageFails() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(
                REQUEST_ID_1, 404, IMAGE_URL_1, new ArrayList<Bitmap>(), new ArrayList<Rect>());

        verify(mCallback).onImageDownloaded((Bitmap) isNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
    }

    @Test
    public void testEmptyImageList() {
        mImages.clear();
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mWebContents, times(0))
                .downloadImage(
                        any(), anyBoolean(), anyInt(), anyBoolean(), any(MediaImageManager.class));
        verify(mCallback).onImageDownloaded((Bitmap) isNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
    }

    @Test
    public void testNullImageList() {
        mMediaImageManager.downloadImage(null, mCallback);

        verify(mWebContents, times(0))
                .downloadImage(
                        any(), anyBoolean(), anyInt(), anyBoolean(), any(MediaImageManager.class));
        verify(mCallback).onImageDownloaded((Bitmap) isNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
    }
}

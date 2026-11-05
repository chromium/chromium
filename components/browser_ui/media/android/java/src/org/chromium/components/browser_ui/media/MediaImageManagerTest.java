// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Rect;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.services.media_session.MediaImage;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Robolectric tests for MediaImageManager. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaImageManagerTest {
    private static final int TINY_IMAGE_SIZE_PX = 50;
    private static final int MIN_IMAGE_SIZE_PX = 100;
    private static final int IDEAL_IMAGE_SIZE_PX = 200;
    private static final GURL IMAGE_URL_1 = JUnitTestGURLs.URL_1;
    private static final GURL IMAGE_URL_2 = JUnitTestGURLs.URL_2;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MediaSession mMediaSession;
    @Mock private MediaImageCallback mCallback;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    private MediaImageManager mMediaImageManager;

    // Prepared data for feeding.
    private ArrayList<MediaImage> mImages;
    private Bitmap mBitmap;

    @Before
    public void setUp() {
        mMediaImageManager = new MediaImageManager(MIN_IMAGE_SIZE_PX, IDEAL_IMAGE_SIZE_PX);
        mMediaImageManager.setMediaSession(mMediaSession);

        mImages = new ArrayList<>();
        mImages.add(new MediaImage(IMAGE_URL_1, "", new ArrayList<>()));

        mBitmap =
                Bitmap.createBitmap(
                        IDEAL_IMAGE_SIZE_PX, IDEAL_IMAGE_SIZE_PX, Bitmap.Config.ARGB_8888);
    }

    @Test
    public void testDownloadImage() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(mImages.get(0)),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(mBitmap);

        verify(mCallback).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDownloadSameImageTwice() {
        // First download.
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(mImages.get(0)),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(mBitmap);

        // Second download.
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mMediaSession, times(1)).getMediaImageBitmap(any(), anyInt(), anyInt(), any());
        verify(mCallback, times(1)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDownloadSameImageTwiceButFailed() {
        // First download.
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(mImages.get(0)),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(null);

        // Second download.
        // The second download request will never be initiated because the src is the same.
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mMediaSession, times(1)).getMediaImageBitmap(any(), anyInt(), anyInt(), any());
        verify(mCallback, times(1)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDownloadDifferentImagesTwice() {
        // First download.
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(mImages.get(0)),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(mBitmap);

        // Second download.
        mImages.clear();
        MediaImage image2 = new MediaImage(IMAGE_URL_2, "", new ArrayList<>());
        mImages.add(image2);

        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(image2),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(mBitmap);

        verify(mCallback, times(2)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDownloadAnotherImageBeforeResponse() {
        // First download.
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(mImages.get(0)),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        Callback<Bitmap> firstCallback = mCallbackCaptor.getValue();

        // Second download.
        mImages.clear();
        MediaImage image2 = new MediaImage(IMAGE_URL_2, "", new ArrayList<>());
        mImages.add(image2);

        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(image2),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        Callback<Bitmap> secondCallback = mCallbackCaptor.getValue();

        // The second reply arrives.
        secondCallback.onResult(mBitmap);

        // The first reply arrives afterwards and should be ignored.
        firstCallback.onResult(mBitmap);

        verify(mCallback, times(1)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testDuplicateResponse() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(mImages.get(0)),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        Callback<Bitmap> callback = mCallbackCaptor.getValue();

        callback.onResult(mBitmap);
        callback.onResult(mBitmap);

        verify(mCallback, times(1)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testWrongResponseId() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        mMediaImageManager.onFinishDownloadImage(999, mBitmap);

        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNull());
    }

    @Test
    public void testTinyImagesRemovedBeforeDownloading() {
        mImages.clear();
        ArrayList<Rect> sizes = new ArrayList<>();
        sizes.add(new Rect(0, 0, TINY_IMAGE_SIZE_PX, TINY_IMAGE_SIZE_PX));
        mImages.add(new MediaImage(IMAGE_URL_1, "", sizes));
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mMediaSession, never()).getMediaImageBitmap(any(), anyInt(), anyInt(), any());
        verify(mCallback).onImageDownloaded((Bitmap) isNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
    }

    @Test
    public void testDownloadImageFails() {
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(mImages.get(0)),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(null);

        verify(mCallback).onImageDownloaded((Bitmap) isNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
    }

    @Test
    public void testEmptyImageList() {
        mImages.clear();
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mMediaSession, never()).getMediaImageBitmap(any(), anyInt(), anyInt(), any());
        verify(mCallback).onImageDownloaded((Bitmap) isNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
    }

    @Test
    public void testNullImageList() {
        mMediaImageManager.downloadImage(null, mCallback);

        verify(mMediaSession, never()).getMediaImageBitmap(any(), anyInt(), anyInt(), any());
        verify(mCallback).onImageDownloaded((Bitmap) isNull());
        verify(mCallback, times(0)).onImageDownloaded((Bitmap) isNotNull());
    }

    @Test
    public void testNullMediaSession() {
        mMediaImageManager.setMediaSession(null);
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mMediaSession, never()).getMediaImageBitmap(any(), anyInt(), anyInt(), any());
        verify(mCallback, never()).onImageDownloaded(any());
    }

    @Test
    public void testSetMediaSessionResetsLastImageSrc() {
        // First download.
        mMediaImageManager.downloadImage(mImages, mCallback);
        verify(mMediaSession)
                .getMediaImageBitmap(
                        eq(mImages.get(0)),
                        eq(MIN_IMAGE_SIZE_PX),
                        eq(IDEAL_IMAGE_SIZE_PX),
                        mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(mBitmap);

        // Resetting the MediaSession should clear the cached last image src, allowing the same
        // image URL to be downloaded again for the new session.
        mMediaImageManager.setMediaSession(mMediaSession);
        mMediaImageManager.downloadImage(mImages, mCallback);

        verify(mMediaSession, times(2)).getMediaImageBitmap(any(), anyInt(), anyInt(), any());
    }
}

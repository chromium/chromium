// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.graphics.Bitmap;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for CachedImageFetcher. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowPostTask.class})
public class CachedImageFetcherTest {
    private static final String UMA_CLIENT_NAME = "TestUmaClient";
    private static final String URL = JUnitTestGURLs.RED_1.getSpec();
    private static final String PATH = "test/path/cache/test.png";
    private static final int WIDTH_PX = 10;
    private static final int HEIGHT_PX = 20;

    @Mock ImageFetcherBridge mBridge;
    @Mock CachedImageFetcher.ImageLoader mImageLoader;
    @Mock BaseGifImage mGif;
    @Mock Callback<Bitmap> mBitmapCallback;
    @Mock Callback<BaseGifImage> mGifCallback;

    CachedImageFetcher mCachedImageFetcher;
    Bitmap mBitmap;

    @Before
    public void setUp() {
        ShadowPostTask.setTestImpl(
                new ShadowPostTask.TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });

        MockitoAnnotations.initMocks(this);

        doReturn(PATH).when(mBridge).getFilePath(URL);
        mCachedImageFetcher = new CachedImageFetcher(mBridge, mImageLoader);

        mBitmap = Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);
        ArgumentCaptor<Callback<Bitmap>> bitmapCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            bitmapCallbackCaptor.getValue().onResult(mBitmap);
                            return null;
                        })
                .when(mBridge)
                .fetchImage(anyInt(), any(), bitmapCallbackCaptor.capture());

        ArgumentCaptor<Callback<BaseGifImage>> gifCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            gifCallbackCaptor.getValue().onResult(mGif);
                            return null;
                        })
                .when(mBridge)
                .fetchGif(
                        anyInt(),
                        eq(ImageFetcher.Params.create(URL, UMA_CLIENT_NAME)),
                        gifCallbackCaptor.capture());
    }

    @Test
    public void testFetchImage_fileNotFoundOnDisk() {
        doReturn(null).when(mImageLoader).tryToLoadImageFromDisk(PATH);

        ImageFetcher.Params params =
                ImageFetcher.Params.create(URL, UMA_CLIENT_NAME, WIDTH_PX, HEIGHT_PX);
        mCachedImageFetcher.fetchImage(params, mBitmapCallback);
        verify(mBitmapCallback).onResult(mBitmap);
        verify(mBridge).fetchImage(eq(ImageFetcherConfig.DISK_CACHE_ONLY), eq(params), any());
    }

    @Test
    public void testFetchImage_fileFoundOnDisk_imageNotResized() {
        doReturn(mBitmap).when(mImageLoader).tryToLoadImageFromDisk(PATH);

        ImageFetcher.Params params =
                ImageFetcher.Params.createNoResizing(
                        new GURL(URL), UMA_CLIENT_NAME, WIDTH_PX + 1, HEIGHT_PX + 1);
        mCachedImageFetcher.fetchImage(params, mBitmapCallback);

        // Unresized bitmap should be returned.
        ArgumentCaptor<Bitmap> bitmapCaptor = ArgumentCaptor.forClass(Bitmap.class);
        verify(mBitmapCallback).onResult(bitmapCaptor.capture());
        Assert.assertEquals(mBitmap, bitmapCaptor.getValue());

        verify(mBridge, never())
                .fetchImage(eq(ImageFetcherConfig.DISK_CACHE_ONLY), eq(params), any());
        verify(mBridge).reportEvent(UMA_CLIENT_NAME, ImageFetcherEvent.JAVA_DISK_CACHE_HIT);
        verify(mBridge).reportCacheHitTime(eq(UMA_CLIENT_NAME), anyLong());
    }

    @Test
    public void testFetchImage_fileFoundOnDisk_imageResized() {
        doReturn(mBitmap).when(mImageLoader).tryToLoadImageFromDisk(PATH);

        ImageFetcher.Params params =
                ImageFetcher.Params.create(URL, UMA_CLIENT_NAME, WIDTH_PX + 1, HEIGHT_PX + 1);
        mCachedImageFetcher.fetchImage(params, mBitmapCallback);

        ArgumentCaptor<Bitmap> bitmapCaptor = ArgumentCaptor.forClass(Bitmap.class);
        verify(mBitmapCallback).onResult(bitmapCaptor.capture());
        Bitmap actual = bitmapCaptor.getValue();
        Assert.assertNotEquals(mBitmap, actual);
        Assert.assertEquals(WIDTH_PX + 1, actual.getWidth());
        Assert.assertEquals(HEIGHT_PX + 1, actual.getHeight());

        verify(mBridge, never())
                .fetchImage(
                        eq(ImageFetcherConfig.DISK_CACHE_ONLY),
                        eq(ImageFetcher.Params.create(URL, UMA_CLIENT_NAME, WIDTH_PX, HEIGHT_PX)),
                        any());
        verify(mBridge).reportEvent(UMA_CLIENT_NAME, ImageFetcherEvent.JAVA_DISK_CACHE_HIT);
        verify(mBridge).reportCacheHitTime(eq(UMA_CLIENT_NAME), anyLong());
    }

    @Test
    public void testFetchGif_fileNotFoundOnDisk() {
        doReturn(null).when(mImageLoader).tryToLoadGifFromDisk(PATH);

        ImageFetcher.Params params = ImageFetcher.Params.create(URL, UMA_CLIENT_NAME);
        mCachedImageFetcher.fetchGif(params, mGifCallback);

        ArgumentCaptor<BaseGifImage> gifCaptor = ArgumentCaptor.forClass(BaseGifImage.class);
        verify(mGifCallback).onResult(gifCaptor.capture());
        Assert.assertEquals(mGif, gifCaptor.getValue());

        verify(mBridge).fetchGif(eq(ImageFetcherConfig.DISK_CACHE_ONLY), eq(params), any());
    }

    @Test
    public void testFetchGif_fileFoundOnDisk() {
        doReturn(mGif).when(mImageLoader).tryToLoadGifFromDisk(PATH);

        ImageFetcher.Params params = ImageFetcher.Params.create(URL, UMA_CLIENT_NAME);
        mCachedImageFetcher.fetchGif(params, mGifCallback);

        ArgumentCaptor<BaseGifImage> gifCaptor = ArgumentCaptor.forClass(BaseGifImage.class);
        verify(mGifCallback).onResult(gifCaptor.capture());
        Assert.assertEquals(mGif, gifCaptor.getValue());

        verify(mBridge, never())
                .fetchGif(eq(ImageFetcherConfig.DISK_CACHE_ONLY), eq(params), any());
        verify(mBridge).reportEvent(UMA_CLIENT_NAME, ImageFetcherEvent.JAVA_DISK_CACHE_HIT);
        verify(mBridge).reportCacheHitTime(eq(UMA_CLIENT_NAME), anyLong());
    }

    @Test
    public void testClear() {
        // Clear does nothing in CachedImageFetcher.
        mCachedImageFetcher.clear();
        verifyNoMoreInteractions(mBridge);
    }

    @Test
    public void testDestroy() {
        // Destroy does nothing in CachedImageFetcher.
        mCachedImageFetcher.destroy();
        verifyNoMoreInteractions(mBridge);
    }

    @Test
    public void testGetConfig() {
        Assert.assertEquals(ImageFetcherConfig.DISK_CACHE_ONLY, mCachedImageFetcher.getConfig());
    }
}

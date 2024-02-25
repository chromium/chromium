// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
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
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Test for NetworkImageFetcher.java. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NetworkImageFetcherTest {
    private static final String UMA_CLIENT_NAME = "TestUmaClient";
    private static final String URL = "http://google.com/test.png";
    private static final int WIDTH_PX = 10;
    private static final int HEIGHT_PX = 20;

    @Mock ImageFetcherBridge mBridge;
    @Mock Callback<Bitmap> mBitmapCallback;
    @Mock Callback<BaseGifImage> mGifCallback;

    NetworkImageFetcher mImageFetcher;
    Bitmap mBitmap;
    BaseGifImage mGif;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mImageFetcher = new NetworkImageFetcher(mBridge);

        mBitmap = Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);
        // This gif won't be valid, but we're only using the address in these tests.
        mGif = new BaseGifImage(new byte[] {});
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
    public void test_fetchImage() {
        ImageFetcher.Params params =
                ImageFetcher.Params.create(URL, UMA_CLIENT_NAME, WIDTH_PX, HEIGHT_PX);
        mImageFetcher.fetchImage(params, mBitmapCallback);
        verify(mBitmapCallback).onResult(mBitmap);
        verify(mBridge).fetchImage(eq(ImageFetcherConfig.NETWORK_ONLY), eq(params), any());
        verify(mBridge).reportTotalFetchTimeFromNative(eq(UMA_CLIENT_NAME), anyLong());
    }

    @Test
    public void test_fetchGif() {
        ImageFetcher.Params params = ImageFetcher.Params.create(URL, UMA_CLIENT_NAME);
        mImageFetcher.fetchGif(params, mGifCallback);
        verify(mGifCallback).onResult(mGif);
        verify(mBridge).fetchGif(ImageFetcherConfig.NETWORK_ONLY, params, mGifCallback);
    }

    @Test
    public void testClear() {
        // Clear does nothing in NetworkImageFetcher.
        mImageFetcher.clear();
        verifyNoMoreInteractions(mBridge);
    }

    @Test
    public void testDestroy() {
        // Destroy does nothing in NetworkImageFetcher.
        mImageFetcher.destroy();
        verifyNoMoreInteractions(mBridge);
    }

    @Test
    public void testGetConfig() {
        Assert.assertEquals(ImageFetcherConfig.NETWORK_ONLY, mImageFetcher.getConfig());
    }
}

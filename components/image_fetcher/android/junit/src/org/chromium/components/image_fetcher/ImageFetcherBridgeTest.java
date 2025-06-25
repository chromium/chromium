// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.embedder_support.simple_factory_key.SimpleFactoryKeyHandle;
import org.chromium.url.JUnitTestGURLs;

/** Test for ImageFetcherBridge.java. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImageFetcherBridgeTest {
    private static final int WIDTH_PX = 10;
    private static final int HEIGHT_PX = 20;
    private static final int EXPIRATION_INTERVAL_MINS = 60;

    @Rule public ExpectedException mExpectedException = ExpectedException.none();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock ImageFetcherBridge.Natives mNatives;
    @Mock SimpleFactoryKeyHandle mSimpleFactoryKeyHandle;
    @Mock Callback<Bitmap> mBitmapCallback;
    @Mock Callback<ImageFetchResult> mImageFetchResultCallback;
    @Mock Callback<ImageDataFetchResult> mGifCallback;

    ImageFetcherBridge mBridge;

    @Before
    public void setUp() {
        ImageFetcherBridgeJni.setInstanceForTesting(mNatives);
        mBridge = new ImageFetcherBridge(mSimpleFactoryKeyHandle);
    }

    @Test
    public void testFetchImage() {
        ArgumentCaptor<Callback<ImageFetchResult>> callbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        final ArgumentCaptor<Bitmap> resultCaptor = ArgumentCaptor.forClass(Bitmap.class);
        final Bitmap bitmap = Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);
        final RequestMetadata requestMetadata =
                new RequestMetadata("image/jpeg", 200, "test_content_location_header");
        final ImageFetchResult imageFetchResult = new ImageFetchResult(bitmap, requestMetadata);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            callbackCaptor.getValue().onResult(imageFetchResult);
                            return null;
                        })
                .when(mNatives)
                .fetchImage(
                        eq(mSimpleFactoryKeyHandle),
                        anyInt(),
                        anyString(),
                        anyString(),
                        eq(WIDTH_PX),
                        eq(HEIGHT_PX),
                        eq(0),
                        callbackCaptor.capture());

        mBridge.fetchImage(
                -1, ImageFetcher.Params.create("", "", WIDTH_PX, HEIGHT_PX), mBitmapCallback);
        verify(mBitmapCallback).onResult(resultCaptor.capture());
        Assert.assertEquals(bitmap, resultCaptor.getValue());
    }

    @Test
    public void testFetchImageWithExpirationInterval() {
        ArgumentCaptor<Callback<ImageFetchResult>> callbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        final ArgumentCaptor<Bitmap> resultCaptor = ArgumentCaptor.forClass(Bitmap.class);
        final Bitmap bitmap = Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);
        final RequestMetadata requestMetadata =
                new RequestMetadata("image/jpeg", 200, "test_content_location_header");
        final ImageFetchResult imageFetchResult = new ImageFetchResult(bitmap, requestMetadata);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            callbackCaptor.getValue().onResult(imageFetchResult);
                            return null;
                        })
                .when(mNatives)
                .fetchImage(
                        eq(mSimpleFactoryKeyHandle),
                        anyInt(),
                        anyString(),
                        anyString(),
                        eq(WIDTH_PX),
                        eq(HEIGHT_PX),
                        eq(EXPIRATION_INTERVAL_MINS),
                        callbackCaptor.capture());

        mBridge.fetchImage(
                -1,
                ImageFetcher.Params.createWithExpirationInterval(
                        JUnitTestGURLs.EXAMPLE_URL,
                        "clientname",
                        WIDTH_PX,
                        HEIGHT_PX,
                        EXPIRATION_INTERVAL_MINS),
                mBitmapCallback);
        verify(mBitmapCallback).onResult(resultCaptor.capture());
        Assert.assertEquals(bitmap, resultCaptor.getValue());
    }

    @Test
    public void testFetchImage_imageResized() {
        int desiredWidth = 100;
        int desiredHeight = 100;

        ArgumentCaptor<Callback<ImageFetchResult>> callbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        final Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        final ImageFetchResult imageFetchResult =
                new ImageFetchResult(
                        bitmap,
                        new RequestMetadata("image/jpeg", 200, "test_content_location_header"));
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            callbackCaptor.getValue().onResult(imageFetchResult);
                            return null;
                        })
                .when(mNatives)
                .fetchImage(
                        eq(mSimpleFactoryKeyHandle),
                        anyInt(),
                        anyString(),
                        anyString(),
                        eq(desiredWidth),
                        eq(desiredHeight),
                        eq(0),
                        callbackCaptor.capture());

        mBridge.fetchImage(
                -1,
                ImageFetcher.Params.create("", "", desiredWidth, desiredHeight),
                mBitmapCallback);
        ArgumentCaptor<Bitmap> bitmapCaptor = ArgumentCaptor.forClass(Bitmap.class);
        verify(mBitmapCallback).onResult(bitmapCaptor.capture());

        Bitmap actual = bitmapCaptor.getValue();
        Assert.assertNotEquals(
                "the bitmap should have been copied when it was resized", bitmap, actual);
        Assert.assertEquals(100, actual.getWidth());
        Assert.assertEquals(100, actual.getHeight());
    }

    @Test
    public void testFetchImageWithRequestMetadata() {
        ArgumentCaptor<Callback<ImageFetchResult>> callbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        final ArgumentCaptor<ImageFetchResult> resultCaptor =
                ArgumentCaptor.forClass(ImageFetchResult.class);
        final Bitmap bitmap = Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);
        final RequestMetadata requestMetadata =
                new RequestMetadata("image/jpeg", 200, "test_content_location_header");
        final ImageFetchResult imageFetchResult = new ImageFetchResult(bitmap, requestMetadata);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            callbackCaptor.getValue().onResult(imageFetchResult);
                            return null;
                        })
                .when(mNatives)
                .fetchImage(
                        eq(mSimpleFactoryKeyHandle),
                        anyInt(),
                        anyString(),
                        anyString(),
                        eq(WIDTH_PX),
                        eq(HEIGHT_PX),
                        eq(0),
                        callbackCaptor.capture());

        mBridge.fetchImageWithRequestMetadata(
                -1,
                ImageFetcher.Params.create("", "", WIDTH_PX, HEIGHT_PX),
                mImageFetchResultCallback);
        verify(mImageFetchResultCallback).onResult(resultCaptor.capture());
        ImageFetchResult actualResult = resultCaptor.getValue();
        Assert.assertNotNull(actualResult);
        Assert.assertEquals(bitmap, actualResult.imageBitmap);
        Assert.assertEquals(requestMetadata, actualResult.requestMetadata);
    }

    @Test
    public void testFetchImageWithRequestMetadataWithExpirationInterval() {
        ArgumentCaptor<Callback<ImageFetchResult>> callbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        final ArgumentCaptor<ImageFetchResult> resultCaptor =
                ArgumentCaptor.forClass(ImageFetchResult.class);
        final Bitmap bitmap = Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);
        final RequestMetadata requestMetadata =
                new RequestMetadata("image/jpeg", 200, "test_content_location_header");
        final ImageFetchResult imageFetchResult = new ImageFetchResult(bitmap, requestMetadata);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            callbackCaptor.getValue().onResult(imageFetchResult);
                            return null;
                        })
                .when(mNatives)
                .fetchImage(
                        eq(mSimpleFactoryKeyHandle),
                        anyInt(),
                        anyString(),
                        anyString(),
                        eq(WIDTH_PX),
                        eq(HEIGHT_PX),
                        eq(EXPIRATION_INTERVAL_MINS),
                        callbackCaptor.capture());

        mBridge.fetchImageWithRequestMetadata(
                -1,
                ImageFetcher.Params.createWithExpirationInterval(
                        JUnitTestGURLs.EXAMPLE_URL,
                        "clientname",
                        WIDTH_PX,
                        HEIGHT_PX,
                        EXPIRATION_INTERVAL_MINS),
                mImageFetchResultCallback);
        verify(mImageFetchResultCallback).onResult(resultCaptor.capture());
        ImageFetchResult actualResult = resultCaptor.getValue();
        Assert.assertNotNull(actualResult);
        Assert.assertEquals(bitmap, actualResult.imageBitmap);
        Assert.assertEquals(requestMetadata, actualResult.requestMetadata);
    }

    @Test
    public void testFetchImageWithRequestMetadata_imageResized() {
        int desiredWidth = 100;
        int desiredHeight = 100;

        ArgumentCaptor<Callback<ImageFetchResult>> callbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        final Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        final RequestMetadata requestMetadata =
                new RequestMetadata("image/jpeg", 200, "test_content_location_header");
        final ImageFetchResult imageFetchResult = new ImageFetchResult(bitmap, requestMetadata);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            callbackCaptor.getValue().onResult(imageFetchResult);
                            return null;
                        })
                .when(mNatives)
                .fetchImage(
                        eq(mSimpleFactoryKeyHandle),
                        anyInt(),
                        anyString(),
                        anyString(),
                        eq(desiredWidth),
                        eq(desiredHeight),
                        eq(0),
                        callbackCaptor.capture());

        mBridge.fetchImageWithRequestMetadata(
                -1,
                ImageFetcher.Params.create("", "", desiredWidth, desiredHeight),
                mImageFetchResultCallback);
        ArgumentCaptor<ImageFetchResult> bitmapCaptor =
                ArgumentCaptor.forClass(ImageFetchResult.class);
        verify(mImageFetchResultCallback).onResult(bitmapCaptor.capture());

        ImageFetchResult actualResult = bitmapCaptor.getValue();
        Assert.assertNotNull(actualResult);
        Assert.assertNotEquals(
                "the bitmap should have been copied when it was resized",
                bitmap,
                actualResult.imageBitmap);
        Assert.assertEquals(requestMetadata, actualResult.requestMetadata);
        Assert.assertEquals(100, actualResult.imageBitmap.getWidth());
        Assert.assertEquals(100, actualResult.imageBitmap.getHeight());
    }

    @Test
    public void testFetchGif() {
        ArgumentCaptor<Callback<ImageDataFetchResult>> callbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        final byte[] imageData = new byte[] {1, 2, 3};
        final RequestMetadata requestMetadata =
                new RequestMetadata("image/gif", 200, "test_content_location_header");
        final ImageDataFetchResult imageDataFetchResult =
                new ImageDataFetchResult(imageData, requestMetadata);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            callbackCaptor.getValue().onResult(imageDataFetchResult);
                            return null;
                        })
                .when(mNatives)
                .fetchImageData(
                        eq(mSimpleFactoryKeyHandle),
                        anyInt(),
                        anyString(),
                        anyString(),
                        eq(0),
                        callbackCaptor.capture());

        mBridge.fetchGif(-1, ImageFetcher.Params.create("", ""), mGifCallback);
        ArgumentCaptor<ImageDataFetchResult> gifCaptor =
                ArgumentCaptor.forClass(ImageDataFetchResult.class);
        verify(mGifCallback).onResult(gifCaptor.capture());
        ImageDataFetchResult actualResult = gifCaptor.getValue();
        Assert.assertNotNull(actualResult);
        Assert.assertEquals(imageData, actualResult.imageData);
        Assert.assertEquals(requestMetadata, actualResult.requestMetadata);
    }

    @Test
    public void testFetchGif_imageDataEmpty() {
        ArgumentCaptor<Callback<ImageDataFetchResult>> callbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        final ImageDataFetchResult imageDataFetchResult =
                new ImageDataFetchResult(
                        new byte[] {},
                        new RequestMetadata("image/jpeg", -1, "test_content_location_header"));
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            callbackCaptor.getValue().onResult(imageDataFetchResult);
                            return null;
                        })
                .when(mNatives)
                .fetchImageData(
                        eq(mSimpleFactoryKeyHandle),
                        anyInt(),
                        anyString(),
                        anyString(),
                        eq(0),
                        callbackCaptor.capture());

        mBridge.fetchGif(-1, ImageFetcher.Params.create("", ""), mGifCallback);
        verify(mGifCallback).onResult(imageDataFetchResult);
    }

    @Test
    public void testGetFilePath() {
        mBridge.getFilePath("testing is cool");
        verify(mNatives).getFilePath(mSimpleFactoryKeyHandle, "testing is cool");
    }

    @Test
    public void testReportEvent() {
        mBridge.reportEvent("client", 10);
        verify(mNatives).reportEvent("client", 10);
    }

    @Test
    public void testReportCacheHitTime() {
        mBridge.reportCacheHitTime("client", 10L);
        verify(mNatives).reportCacheHitTime("client", 10L);
    }

    @Test
    public void testReportTotalFetchTimeFromNative() {
        mBridge.reportTotalFetchTimeFromNative("client", 10L);
        verify(mNatives).reportTotalFetchTimeFromNative("client", 10L);
    }

    @Test
    public void testSetupForTesting() {
        // Since ImageFetcherBridge creates different instance on each call of getForProfile
        // function, two instances below should not be equal.
        Assert.assertNotEquals(
                mBridge, ImageFetcherBridge.getForSimpleFactoryKeyHandle(mSimpleFactoryKeyHandle));
    }
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.image_fetcher;

import static org.junit.Assume.assumeFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.util.BitmapCache;

/** Unit tests for InMemoryCachedImageFetcher. */
@SuppressWarnings("unchecked")
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InMemoryCachedImageFetcherTest {
    private static final String UMA_CLIENT_NAME = "TestUmaClient";
    private static final String URL = "http://foo.bar";
    private static final int WIDTH_PX = 100;
    private static final int HEIGHT_PX = 200;
    private static final int DEFAULT_CACHE_SIZE = 100;
    private static final int UNKNOWN_IMAGE_FETCHER_CONFIG = -1;

    @Rule public ExpectedException mExpectedException = ExpectedException.none();

    private final Bitmap mBitmap =
            Bitmap.createBitmap(WIDTH_PX, HEIGHT_PX, Bitmap.Config.ARGB_8888);

    // The image fetcher under test.
    private InMemoryCachedImageFetcher mInMemoryCachedImageFetcher;
    private BitmapCache mBitmapCache;
    private DiscardableReferencePool mReferencePool;

    @Mock private ImageFetcherBridge mBridge;
    @Mock private CachedImageFetcher mMockImageFetcher;
    @Mock private Callback<Bitmap> mCallback;
    @Mock private Runtime mRuntime;
    @Captor private ArgumentCaptor<Integer> mWidthCaptor;
    @Captor private ArgumentCaptor<Integer> mHeightCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mBridge).when(mMockImageFetcher).getImageFetcherBridge();

        mReferencePool = new DiscardableReferencePool();
        mBitmapCache = Mockito.spy(new BitmapCache(mReferencePool, DEFAULT_CACHE_SIZE));
        mInMemoryCachedImageFetcher =
                new InMemoryCachedImageFetcher(mMockImageFetcher, mBitmapCache);
    }

    @After
    public void tearDown() {
        mInMemoryCachedImageFetcher.destroy();
    }

    private void answerFetch(Bitmap bitmap, boolean deleteBitmapCacheOnFetch) {
        mInMemoryCachedImageFetcher =
                new InMemoryCachedImageFetcher(mMockImageFetcher, mBitmapCache);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            if (deleteBitmapCacheOnFetch) {
                                mInMemoryCachedImageFetcher.destroy();
                                mReferencePool.drain();
                            }

                            mCallbackCaptor.getValue().onResult(bitmap);
                            return null;
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), mCallbackCaptor.capture());
    }

    // Use with junit.Assume to turn assertions on/off for specific test.
    private boolean assertionsEnabled() {
        return InMemoryCachedImageFetcherTest.class.desiredAssertionStatus();
    }

    @Test
    public void testConstructor_unknownConfig() {
        assumeFalse(assertionsEnabled());
        doReturn(UNKNOWN_IMAGE_FETCHER_CONFIG).when(mMockImageFetcher).getConfig();
        mInMemoryCachedImageFetcher =
                new InMemoryCachedImageFetcher(mMockImageFetcher, mBitmapCache);
        Assert.assertEquals(
                ImageFetcherConfig.IN_MEMORY_ONLY, mInMemoryCachedImageFetcher.getConfig());
    }

    @Test
    public void testFetchImageCachesFirstCall() {
        answerFetch(mBitmap, false);
        ImageFetcher.Params params =
                ImageFetcher.Params.create(URL, UMA_CLIENT_NAME, WIDTH_PX, HEIGHT_PX);
        mInMemoryCachedImageFetcher.fetchImage(params, mCallback);
        verify(mCallback).onResult(mBitmap);

        reset(mCallback);
        mInMemoryCachedImageFetcher.fetchImage(params, mCallback);
        verify(mCallback).onResult(mBitmap);

        verify(mMockImageFetcher).fetchImage(eq(params), any());

        // Verify metrics are reported.
        verify(mBridge).reportEvent(UMA_CLIENT_NAME, ImageFetcherEvent.JAVA_IN_MEMORY_CACHE_HIT);
    }

    @Test
    public void testFetchImageDoesNotCacheAfterDestroy() {
        try {
            answerFetch(mBitmap, true);

            // No exception should be thrown here when bitmap cache is null.
            ImageFetcher.Params params =
                    ImageFetcher.Params.create(URL, UMA_CLIENT_NAME, WIDTH_PX, HEIGHT_PX);
            mInMemoryCachedImageFetcher.fetchImage(params, (Bitmap bitmap) -> {});
        } catch (Exception e) {
            throw new AssertionError(
                    "Destroy called in the middle of execution shouldn't throw", e);
        }
    }

    @Test
    public void testFetchGif() {
        ImageFetcher.Params params = ImageFetcher.Params.create(URL, UMA_CLIENT_NAME);
        mInMemoryCachedImageFetcher.fetchGif(params, (BaseGifImage gif) -> {});
        verify(mMockImageFetcher).fetchGif(eq(params), any());
    }

    @Test
    public void testClear() {
        mInMemoryCachedImageFetcher =
                new InMemoryCachedImageFetcher(mMockImageFetcher, mBitmapCache);
        mInMemoryCachedImageFetcher.clear();

        verify(mBitmapCache).clear();
    }

    @Test
    public void testDestroy() {
        mInMemoryCachedImageFetcher.destroy();

        verify(mMockImageFetcher).destroy();
        verify(mBitmapCache).destroy();

        // Check that calling methods after destroy throw AssertionErrors.
        mExpectedException.expect(AssertionError.class);
        mExpectedException.expectMessage("fetchGif called after destroy");
        mInMemoryCachedImageFetcher.fetchGif(ImageFetcher.Params.create("", ""), null);
        mExpectedException.expectMessage("fetchImage called after destroy");
        mInMemoryCachedImageFetcher.fetchImage(ImageFetcher.Params.create("", "", 100, 100), null);
        mExpectedException.expectMessage("clear called after destroy");
        mInMemoryCachedImageFetcher.clear();
    }

    @Test
    public void testEncodeCacheKey() {
        Assert.assertEquals(
                "url/1/100/200",
                mInMemoryCachedImageFetcher.encodeCacheKey(
                        "url", /* shouldResize= */ true, 100, 200));
    }

    @Test
    public void testDetermineCacheSize_clientRequestedSmallerThanAvailable() {
        long totalMemory = 200L;
        long allocatedMemory = 100L;
        int clientRequest = 10;
        doReturn(totalMemory).when(mRuntime).maxMemory();
        doReturn(allocatedMemory).when(mRuntime).totalMemory();
        doReturn(0L).when(mRuntime).freeMemory();

        // We calculate the in-memory cache size as a percentage of available memory.
        Assert.assertEquals(
                "Cache size should be bounded by the space requested by the client.",
                clientRequest,
                InMemoryCachedImageFetcher.determineCacheSize(mRuntime, clientRequest));
    }

    @Test
    public void testDetermineCacheSize_clientRequestedLargerThanAvailable() {
        long totalMemory = 200L;
        long allocatedMemory = 120L;
        int clientRequest = 100;
        doReturn(totalMemory).when(mRuntime).maxMemory();
        doReturn(allocatedMemory).when(mRuntime).totalMemory();
        doReturn(0L).when(mRuntime).freeMemory();

        // We calculate the in-memory cache size as a percentage of available memory.
        Assert.assertEquals(
                "Client requests should be bounded by 1/8th of the available memory.",
                10,
                InMemoryCachedImageFetcher.determineCacheSize(mRuntime, clientRequest));
    }

    @Test
    public void testDetermineCacheSize_freeMemoryLowerBound() {
        long totalMemory = 200L;
        long allocatedMemory = 199L;
        int clientRequest = 10;
        doReturn(totalMemory).when(mRuntime).maxMemory();
        doReturn(allocatedMemory).when(mRuntime).totalMemory();
        doReturn(0L).when(mRuntime).freeMemory();

        // We calculate the in-memory cache size as a percentage of available memory.
        Assert.assertEquals(
                "The minimum cache size is 1.",
                1,
                InMemoryCachedImageFetcher.determineCacheSize(mRuntime, clientRequest));
    }
}

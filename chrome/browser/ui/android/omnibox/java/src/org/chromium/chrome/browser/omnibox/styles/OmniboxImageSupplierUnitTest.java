// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link OmniboxImageSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class OmniboxImageSupplierUnitTest {
    private static final GURL NAV_URL = JUnitTestGURLs.URL_1;
    private static final GURL NAV_URL_2 = JUnitTestGURLs.URL_2;
    private static final int FALLBACK_COLOR = 0xACE0BA5E;

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();

    private ArgumentCaptor<LargeIconCallback> mIconCallbackCaptor =
            ArgumentCaptor.forClass(LargeIconCallback.class);

    private OmniboxImageSupplier mSupplier;

    private @Mock Bitmap mBitmap1;
    private @Mock Bitmap mBitmap2;
    private @Mock RoundedIconGenerator mIconGenerator;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeJni;
    private @Mock Callback<Bitmap> mCallback1;
    private @Mock Callback<Bitmap> mCallback2;
    private @Mock Profile mProfile;
    private @Mock ImageFetcher mImageFetcher;
    private @Px int mFaviconSize;

    @Before
    public void setUp() {
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeJni);

        var context = ContextUtils.getApplicationContext();
        mFaviconSize =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_favicon_size);
        assert mFaviconSize != 0;
        mSupplier = new OmniboxImageSupplier(context);
        mSupplier.setRoundedIconGeneratorForTesting(mIconGenerator);

        doReturn(1L).when(mLargeIconBridgeJni).init();
        doReturn(true)
                .when(mLargeIconBridgeJni)
                .getLargeIconForURL(anyLong(), any(), any(), anyInt(), anyInt(), any());
    }

    /**
     * Confirm that icon of expected size was requested from LargeIconBridge, and report a supplied
     * bitmap back to the caller.
     *
     * @param url the url to expect a lookup for
     * @param bitmap the bitmap to return to the caller (may be null)
     */
    private void verifyLargeIconBridgeRequest(@NonNull GURL url, @Nullable Bitmap bitmap) {
        ShadowLooper.runUiThreadTasks();
        verify(mLargeIconBridgeJni)
                .getLargeIconForURL(
                        anyLong(),
                        eq(mProfile),
                        eq(url),
                        eq(mFaviconSize / 2),
                        eq(mFaviconSize),
                        mIconCallbackCaptor.capture());
        mIconCallbackCaptor.getValue().onLargeIconAvailable(bitmap, FALLBACK_COLOR, true, 0);
    }

    /**
     * Confirm the type of icon reported to the caller.
     *
     * @param bitmap The expected bitmap.
     */
    private void verifyReturnedIcon(@Nullable Bitmap bitmap) {
        verify(mCallback1, times(1)).onResult(eq(bitmap));
    }

    /**
     * Confirm no unexpected calls were made to any of our data producers or consumers and clear all
     * counters.
     */
    private void verifyNoOtherInteractionsAndClearInteractions() {
        verifyNoMoreInteractions(mLargeIconBridgeJni);
        clearInvocations(mLargeIconBridgeJni);

        verifyNoMoreInteractions(mIconGenerator, mCallback1);
        clearInvocations(mIconGenerator, mCallback1);
    }

    @Test
    public void fetchFavicon_noLargeIconBridge() {
        // Favicon service does not exist, so we should expect a _single_ call to
        // RoundedIconGenerator.
        mSupplier.fetchFavicon(NAV_URL, mCallback1);
        verifyReturnedIcon(null);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void generateFavicon_beforeNativeInitialized() {
        doReturn(mBitmap1).when(mIconGenerator).generateIconForUrl(NAV_URL);

        mSupplier.generateFavicon(NAV_URL, mCallback1);
        ShadowLooper.runUiThreadTasks();

        verifyReturnedIcon(null);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void generateFavicon_afterNativeInitialized() {
        doReturn(mBitmap1).when(mIconGenerator).generateIconForUrl(NAV_URL);

        mSupplier.onNativeInitialized();
        mSupplier.generateFavicon(NAV_URL, mCallback1);
        ShadowLooper.runUiThreadTasks();

        verify(mIconGenerator, times(1)).generateIconForUrl(NAV_URL);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_largeIconAvailableWithNoBackoff() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni, times(1)).init();
        mSupplier.fetchFavicon(NAV_URL, mCallback1);
        verifyLargeIconBridgeRequest(NAV_URL, mBitmap1);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try again, expect icon to be served from LargeIconBridge cache.
        mSupplier.fetchFavicon(NAV_URL, mCallback1);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_differentUrlsDontCollide() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni, times(1)).init();

        mSupplier.fetchFavicon(NAV_URL, mCallback1);
        verifyLargeIconBridgeRequest(NAV_URL, mBitmap1);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try another URL. Expect icon to *not* be served from any caches.
        mSupplier.fetchFavicon(NAV_URL_2, mCallback1);
        verifyLargeIconBridgeRequest(NAV_URL_2, mBitmap1);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_clearingCacheRestartsEntireFlow() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni, times(1)).init();
        mSupplier.fetchFavicon(NAV_URL, mCallback1);
        verifyLargeIconBridgeRequest(NAV_URL, mBitmap1);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();

        mSupplier.resetCache();

        // Retry. Expect the exact same flow with that same URL.
        mSupplier.fetchFavicon(NAV_URL, mCallback1);
        verifyLargeIconBridgeRequest(NAV_URL, mBitmap2);
        verifyReturnedIcon(mBitmap2);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void destroy_releasesLargeIconBridgeIfSet() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni, times(1)).init();
        verifyNoMoreInteractions(mLargeIconBridgeJni);
        mSupplier.destroy();
        verify(mLargeIconBridgeJni, times(1)).destroy(anyLong());
        verifyNoMoreInteractions(mLargeIconBridgeJni);
    }

    @Test
    public void setProfile_destroysOldLargeIconBridgeIfPresent() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni, times(1)).init();
        verifyNoMoreInteractions(mLargeIconBridgeJni);
        clearInvocations(mLargeIconBridgeJni);

        // We technically don't expect the change to be "to the same profile", we don't check for
        // this.
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni, times(1)).destroy(anyLong());
        verify(mLargeIconBridgeJni, times(1)).init();
        verifyNoMoreInteractions(mLargeIconBridgeJni);
    }

    @Test
    public void resetCache_safeWhenBridgeAndFetcherNotAvailable() {
        mSupplier.resetCache();
    }

    @Test
    public void resetCache_clearsPendingQueues() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        mSupplier.resetCache();
        verify(mImageFetcher, times(1)).clear();
    }

    @Test
    public void fetchImage_aggregateMultipleRequestsForSameUrl_successfulFetch() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url = JUnitTestGURLs.RED_1;

        // Issue 2 requests for the same URL.
        mSupplier.fetchImage(url, mCallback1);
        mSupplier.fetchImage(url, mCallback2);

        // Observe only one interaction with ImageFetcher.
        ArgumentCaptor<ImageFetcher.Params> paramCaptor =
                ArgumentCaptor.forClass(ImageFetcher.Params.class);
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher, times(1)).fetchImage(paramCaptor.capture(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher);

        // Confirm the URL and no callbacks emitted to registered callbacks.
        assertEquals(JUnitTestGURLs.RED_1.getSpec(), paramCaptor.getValue().url);
        verifyNoMoreInteractions(mCallback1, mCallback2);

        // Emit reply.
        callbackCaptor.getValue().onResult(mBitmap1);

        // Observe all listeners receiving notification.
        verify(mCallback1, times(1)).onResult(mBitmap1);
        verify(mCallback2, times(1)).onResult(mBitmap1);
    }

    @Test
    public void fetchImage_aggregateMultipleRequestsForSameUrl_failingFetch() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url = JUnitTestGURLs.RED_1;

        // Issue 2 requests for the same URL.
        mSupplier.fetchImage(url, mCallback1);
        mSupplier.fetchImage(url, mCallback2);

        // Observe only one interaction with ImageFetcher.
        ArgumentCaptor<ImageFetcher.Params> paramCaptor =
                ArgumentCaptor.forClass(ImageFetcher.Params.class);
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher, times(1)).fetchImage(paramCaptor.capture(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher);

        // Confirm the URL and no callbacks emitted to registered callbacks.
        assertEquals(JUnitTestGURLs.RED_1.getSpec(), paramCaptor.getValue().url);
        verifyNoMoreInteractions(mCallback1, mCallback2);

        // Emit reply.
        callbackCaptor.getValue().onResult(null);

        // Observe no listeners receiving notification.
        verifyNoMoreInteractions(mCallback1, mCallback2);

        // At this point listeners should be removed. Confirm this by emitting a fake second reply
        // and confirm callbacks receiving no call.
        callbackCaptor.getValue().onResult(mBitmap1);
        verifyNoMoreInteractions(mCallback1, mCallback2);
    }

    @Test
    public void fetchImage_aggregateMultipleRequestsForSameUrl_noFetcher() {
        mSupplier.setImageFetcherForTesting(null);

        var url = JUnitTestGURLs.RED_1;

        // Issue 2 requests for the same URL.
        mSupplier.fetchImage(url, mCallback1);
        mSupplier.fetchImage(url, mCallback2);

        verifyNoMoreInteractions(mImageFetcher);

        // Observe listeners receiving no notification.
        verifyNoMoreInteractions(mCallback1, mCallback2);
    }

    @Test
    public void fetchImage_callbacksAreNotRetainedAfterCompletion() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        var url = JUnitTestGURLs.RED_1;

        // Issue first request and observe the interaction with ImageFetcher.
        mSupplier.fetchImage(url, mCallback1);
        verify(mImageFetcher, times(1)).fetchImage(any(), callbackCaptor.capture());

        // Resolve the image. Observe only the first callback receives notification.
        callbackCaptor.getValue().onResult(mBitmap1);
        verify(mCallback1, times(1)).onResult(mBitmap1);
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);

        // Issue second request. Observe second interaction with ImageFetcher.
        mSupplier.fetchImage(url, mCallback2);
        verify(mImageFetcher, times(2)).fetchImage(any(), callbackCaptor.capture());

        // Resolve the image. Observe only the second callback receives notification.
        callbackCaptor.getValue().onResult(mBitmap2);
        verify(mCallback2, times(1)).onResult(mBitmap2);
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);
    }

    @Test
    public void fetchImage_requestsForNonOverlappingUrlsAreNotAggregated() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url1 = JUnitTestGURLs.RED_1;
        var url2 = JUnitTestGURLs.RED_2;
        ArgumentCaptor<Callback<Bitmap>> captor1 = ArgumentCaptor.forClass(Callback.class);
        ArgumentCaptor<Callback<Bitmap>> captor2 = ArgumentCaptor.forClass(Callback.class);

        // Issue 2 requests for two different URLs.
        mSupplier.fetchImage(url1, mCallback1);
        verify(mImageFetcher, times(1)).fetchImage(any(), captor1.capture());
        mSupplier.fetchImage(url2, mCallback2);
        verify(mImageFetcher, times(2)).fetchImage(any(), captor2.capture());
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);

        // Emit first reply.
        captor1.getValue().onResult(mBitmap1);
        verify(mCallback1, times(1)).onResult(mBitmap1);
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);

        // Emit second reply.
        captor2.getValue().onResult(mBitmap2);
        verify(mCallback2, times(1)).onResult(mBitmap2);
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);
    }

    @Test
    public void fetchImage_resultsAfterResetAreDiscarded() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url = JUnitTestGURLs.RED_1;

        mSupplier.fetchImage(url, mCallback1);

        // Observe only one interaction with ImageFetcher.
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher, times(1)).fetchImage(any(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher, mCallback1);

        // Simulate end of Omnibox interaction.
        mSupplier.resetCache();
        verify(mImageFetcher, times(1)).clear();
        verifyNoMoreInteractions(mImageFetcher, mCallback1);

        // Emit late reply. The callback should not be delivered.
        callbackCaptor.getValue().onResult(mBitmap1);
        verifyNoMoreInteractions(mImageFetcher, mCallback1);
    }

    @Test
    public void fetchImage_resultsAfterProfileSwitchAreDiscarded() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url = JUnitTestGURLs.RED_1;

        mSupplier.fetchImage(url, mCallback1);

        // Observe only one interaction with ImageFetcher.
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher, times(1)).fetchImage(any(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher, mCallback1);

        // Simulate end of Omnibox interaction.
        mSupplier.setProfile(mProfile);
        verify(mImageFetcher, times(1)).clear();
        verify(mImageFetcher, times(1)).destroy();
        verifyNoMoreInteractions(mImageFetcher, mCallback1);

        // Emit late reply. The callback should not be delivered.
        callbackCaptor.getValue().onResult(mBitmap1);
        verifyNoMoreInteractions(mImageFetcher, mCallback1);
    }

    @Test
    public void fetchImage_resultsAfterDestroyAreDiscarded() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url = JUnitTestGURLs.RED_1;

        mSupplier.fetchImage(url, mCallback1);

        // Observe only one interaction with ImageFetcher.
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher, times(1)).fetchImage(any(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher, mCallback1);

        mSupplier.destroy();
        verify(mImageFetcher, times(1)).destroy();
        verifyNoMoreInteractions(mImageFetcher, mCallback1);

        // Emit late reply. The callback should not be delivered.
        callbackCaptor.getValue().onResult(mBitmap1);
        verifyNoMoreInteractions(mImageFetcher, mCallback1);
    }

    @Test
    public void fetchImage_emptyUrlsAreRejected() {
        mSupplier.setImageFetcherForTesting(null);

        var url = GURL.emptyGURL();

        // Issue 2 requests for the same URL.
        mSupplier.fetchImage(url, mCallback1);
        mSupplier.fetchImage(url, mCallback2);

        verifyNoMoreInteractions(mImageFetcher);

        // Observe listeners receiving no notification.
        verifyNoMoreInteractions(mCallback1, mCallback2);
    }

    @Test
    public void fetchImage_invalidUrlsAreRejected() {
        mSupplier.setImageFetcherForTesting(null);

        var url = JUnitTestGURLs.INVALID_URL;

        // Issue 2 requests for the same URL.
        mSupplier.fetchImage(url, mCallback1);
        mSupplier.fetchImage(url, mCallback2);

        verifyNoMoreInteractions(mImageFetcher);

        // Observe listeners receiving no notification.
        verifyNoMoreInteractions(mCallback1, mCallback2);
    }
}

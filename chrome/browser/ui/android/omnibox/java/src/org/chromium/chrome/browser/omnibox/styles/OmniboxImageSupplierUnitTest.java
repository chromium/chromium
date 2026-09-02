// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.ui.test.util.MockitoHelper.clearInvocations;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;

import androidx.annotation.Nullable;
import androidx.annotation.Px;

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
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link OmniboxImageSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class OmniboxImageSupplierUnitTest {
    private static final GURL NAV_URL = JUnitTestGURLs.URL_1;
    private static final GURL NAV_URL_2 = JUnitTestGURLs.URL_2;
    private static final int FALLBACK_COLOR = 0xACE0BA5E;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Bitmap mBitmap1;
    @Mock private Bitmap mBitmap2;
    @Mock private LargeIconBridge.Natives mLargeIconBridgeJni;
    @Mock private UrlFormatter.Natives mUrlFormatterJni;
    @Mock private Callback<Drawable> mCallback1;
    @Mock private Callback<Drawable> mCallback2;
    @Mock private Profile mProfile;
    @Mock private ImageFetcher mImageFetcher;
    @Captor private ArgumentCaptor<LargeIconCallback> mIconCallbackCaptor;
    @Captor private ArgumentCaptor<Drawable> mDrawableCaptor;
    @Captor private ArgumentCaptor<ImageFetcher.Params> mParamCaptor;

    private OmniboxImageSupplier mSupplier;
    private @Px int mFaviconSize;

    @Before
    public void setUp() {
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);
        UrlFormatterJni.setInstanceForTesting(mUrlFormatterJni);

        var context = ContextUtils.getApplicationContext();
        mFaviconSize =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_favicon_size);
        assertThat(mFaviconSize).isNotEqualTo(0);
        mSupplier = new OmniboxImageSupplier(context);

        doReturn(1L).when(mLargeIconBridgeJni).init();
        doReturn(true)
                .when(mLargeIconBridgeJni)
                .getLargeIconForURL(anyLong(), any(), any(), anyInt(), anyInt(), any());
        doAnswer(
                        invocation -> {
                            String url = invocation.getArgument(0);
                            if (url.contains("one.com")) {
                                return "www.one.com";
                            }
                            if (url.contains("two.com")) {
                                return "www.two.com";
                            }
                            return url;
                        })
                .when(mUrlFormatterJni)
                .formatUrlForDisplayOmitScheme(any());
    }

    /**
     * Confirm that icon of expected size was requested from LargeIconBridge, and report a supplied
     * bitmap back to the caller.
     *
     * @param url the url to expect a lookup for
     * @param bitmap the bitmap to return to the caller (may be null)
     */
    private void verifyLargeIconBridgeRequest(GURL url, @Nullable Bitmap bitmap) {
        RobolectricUtil.runAllBackgroundAndUi();
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
        if (bitmap == null) {
            verify(mCallback1).onResult(eq(null));
        } else {
            verifyReturnedImage(mCallback1, bitmap);
        }
    }

    private void verifyReturnedImage(Callback<Drawable> callback, Bitmap expectedBitmap) {
        ArgumentCaptor<Drawable> captor = ArgumentCaptor.forClass(Drawable.class);
        verify(callback).onResult(captor.capture());
        assertThat(captor.getValue()).isInstanceOf(BitmapDrawable.class);
        assertThat(((BitmapDrawable) captor.getValue()).getBitmap()).isEqualTo(expectedBitmap);
    }

    /**
     * Confirm no unexpected calls were made to any of our data producers or consumers and clear all
     * counters.
     */
    private void verifyNoOtherInteractionsAndClearInteractions() {
        verifyNoMoreInteractions(mLargeIconBridgeJni);
        clearInvocations(mLargeIconBridgeJni);

        verifyNoMoreInteractions(mCallback1);
        clearInvocations(mCallback1);
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
        mSupplier.generateFavicon(NAV_URL, mCallback1);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback1).onResult(eq(null));
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void generateFavicon_afterNativeInitialized() {
        mSupplier.onNativeInitialized();
        mSupplier.generateFavicon(NAV_URL, mCallback1);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback1).onResult(mDrawableCaptor.capture());
        Drawable drawable = mDrawableCaptor.getValue();
        assertThat(drawable).isInstanceOf(LayerDrawable.class);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void generateFavicon_globe() {
        mSupplier.onNativeInitialized();
        mSupplier.generateFavicon(NAV_URL, OmniboxImageSupplier.FallbackIconType.GLOBE, mCallback1);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mCallback1).onResult(mDrawableCaptor.capture());
        assertThat(mDrawableCaptor.getValue()).isNotNull();
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_largeIconAvailableWithNoBackoff() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni).init();
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
        verify(mLargeIconBridgeJni).init();

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
        verify(mLargeIconBridgeJni).init();
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
        verify(mLargeIconBridgeJni).init();
        verifyNoMoreInteractions(mLargeIconBridgeJni);
        mSupplier.destroy();
        verify(mLargeIconBridgeJni).destroy(anyLong());
        verifyNoMoreInteractions(mLargeIconBridgeJni);
    }

    @Test
    public void setProfile_destroysOldLargeIconBridgeIfPresent() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni).init();
        verifyNoMoreInteractions(mLargeIconBridgeJni);
        clearInvocations(mLargeIconBridgeJni);

        // We technically don't expect the change to be "to the same profile", we don't check for
        // this.
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni).destroy(anyLong());
        verify(mLargeIconBridgeJni).init();
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
        verify(mImageFetcher).clear();
    }

    @Test
    public void fetchImage_aggregateMultipleRequestsForSameUrl_successfulFetch() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url = JUnitTestGURLs.RED_1;

        // Issue 2 requests for the same URL.
        mSupplier.fetchImage(url, mCallback1);
        mSupplier.fetchImage(url, mCallback2);

        // Observe only one interaction with ImageFetcher.
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mImageFetcher).fetchImage(mParamCaptor.capture(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher);

        // Confirm the URL and no callbacks emitted to registered callbacks.
        assertEquals(JUnitTestGURLs.RED_1.getSpec(), mParamCaptor.getValue().url);
        verifyNoMoreInteractions(mCallback1, mCallback2);

        // Emit reply.
        callbackCaptor.getValue().onResult(mBitmap1);

        // Observe all listeners receiving notification.
        verifyReturnedImage(mCallback1, mBitmap1);
        verifyReturnedImage(mCallback2, mBitmap1);
    }

    @Test
    public void fetchImage_aggregateMultipleRequestsForSameUrl_failingFetch() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url = JUnitTestGURLs.RED_1;

        // Issue 2 requests for the same URL.
        mSupplier.fetchImage(url, mCallback1);
        mSupplier.fetchImage(url, mCallback2);

        // Observe only one interaction with ImageFetcher.
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mImageFetcher).fetchImage(mParamCaptor.capture(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher);

        // Confirm the URL and no callbacks emitted to registered callbacks.
        assertEquals(JUnitTestGURLs.RED_1.getSpec(), mParamCaptor.getValue().url);
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

        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = MockitoHelper.callbackCaptor();
        var url = JUnitTestGURLs.RED_1;

        // Issue first request and observe the interaction with ImageFetcher.
        mSupplier.fetchImage(url, mCallback1);
        verify(mImageFetcher).fetchImage(any(), callbackCaptor.capture());

        // Resolve the image. Observe only the first callback receives notification.
        callbackCaptor.getValue().onResult(mBitmap1);
        verifyReturnedImage(mCallback1, mBitmap1);
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);

        // Issue second request. Observe second interaction with ImageFetcher.
        mSupplier.fetchImage(url, mCallback2);
        verify(mImageFetcher, times(2)).fetchImage(any(), callbackCaptor.capture());

        // Resolve the image. Observe only the second callback receives notification.
        callbackCaptor.getValue().onResult(mBitmap2);
        verifyReturnedImage(mCallback2, mBitmap2);
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);
    }

    @Test
    public void fetchImage_requestsForNonOverlappingUrlsAreNotAggregated() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url1 = JUnitTestGURLs.RED_1;
        var url2 = JUnitTestGURLs.RED_2;
        ArgumentCaptor<Callback<Bitmap>> captor1 = MockitoHelper.callbackCaptor();
        ArgumentCaptor<Callback<Bitmap>> captor2 = MockitoHelper.callbackCaptor();

        // Issue 2 requests for two different URLs.
        mSupplier.fetchImage(url1, mCallback1);
        verify(mImageFetcher).fetchImage(any(), captor1.capture());
        mSupplier.fetchImage(url2, mCallback2);
        verify(mImageFetcher, times(2)).fetchImage(any(), captor2.capture());
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);

        // Emit first reply.
        captor1.getValue().onResult(mBitmap1);
        verifyReturnedImage(mCallback1, mBitmap1);
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);

        // Emit second reply.
        captor2.getValue().onResult(mBitmap2);
        verifyReturnedImage(mCallback2, mBitmap2);
        verifyNoMoreInteractions(mImageFetcher, mCallback1, mCallback2);
    }

    @Test
    public void fetchImage_resultsAfterResetAreDiscarded() {
        mSupplier.setImageFetcherForTesting(mImageFetcher);

        var url = JUnitTestGURLs.RED_1;

        mSupplier.fetchImage(url, mCallback1);

        // Observe only one interaction with ImageFetcher.
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mImageFetcher).fetchImage(any(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher, mCallback1);

        // Simulate end of Omnibox interaction.
        mSupplier.resetCache();
        verify(mImageFetcher).clear();
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
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mImageFetcher).fetchImage(any(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher, mCallback1);

        // Simulate end of Omnibox interaction.
        mSupplier.setProfile(mProfile);
        verify(mImageFetcher).clear();
        verify(mImageFetcher).destroy();
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
        ArgumentCaptor<Callback<Bitmap>> callbackCaptor = MockitoHelper.callbackCaptor();
        verify(mImageFetcher).fetchImage(any(), callbackCaptor.capture());
        verifyNoMoreInteractions(mImageFetcher, mCallback1);

        mSupplier.destroy();
        verify(mImageFetcher).destroy();
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

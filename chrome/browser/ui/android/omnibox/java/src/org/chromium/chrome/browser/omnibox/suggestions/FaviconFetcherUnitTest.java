// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher.FaviconFetchCompleteListener;
import org.chromium.chrome.browser.omnibox.suggestions.FaviconFetcher.FaviconType;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests for {@link FaviconFetcher}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowLooper.class})
public final class FaviconFetcherUnitTest {
    private static final GURL NAV_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL NAV_URL_2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
    private static final int FALLBACK_COLOR = 0xACE0BA5E;
    private static final int REGULAR_FAVICON_SIZE_PX = 100;
    private static final int SMALL_FAVICON_SIZE_PX = REGULAR_FAVICON_SIZE_PX / 2;

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    public @Rule ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private ArgumentCaptor<LargeIconCallback> mIconCallbackCaptor =
            ArgumentCaptor.forClass(LargeIconCallback.class);

    private FaviconFetcher mFetcher;

    private @Mock Bitmap mGeneratedIconBitmap;
    private @Mock Bitmap mFavIconBitmap;
    private @Mock LargeIconBridge mLargeIconBridge;
    private @Mock RoundedIconGenerator mIconGenerator;
    private @Mock FaviconFetchCompleteListener mCallback;

    @Before
    public void setUp() {
        // Enable logs to be printed along with possible test failures.
        ShadowLog.stream = System.out;

        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mFetcher = new FaviconFetcher(mActivity, () -> mLargeIconBridge);
        mFetcher.setRoundedIconGeneratorForTesting(mIconGenerator);
        mFetcher.setDesiredFaviconSizeForTesting(REGULAR_FAVICON_SIZE_PX);

        when(mIconGenerator.generateIconForUrl(any(GURL.class))).thenReturn(mGeneratedIconBitmap);
        when(mLargeIconBridge.getLargeIconForUrl(any(), anyInt(), mIconCallbackCaptor.capture()))
                .thenReturn(true);
    }

    /**
     * Confirm that icon of expected size was requested from LargeIconBridge, and report a
     * supplied bitmap back to the caller.
     *
     * @param bitmap The bitmap to return to the caller (may be null).
     */
    private void verifyLargeIconBridgeRequest(
            @NonNull GURL url, @Px int size, @Nullable Bitmap bitmap) {
        ShadowLooper.runUiThreadTasks();
        verify(mLargeIconBridge, times(1))
                .getLargeIconForUrl(eq(url), eq(size), mIconCallbackCaptor.capture());
        mIconCallbackCaptor.getValue().onLargeIconAvailable(bitmap, FALLBACK_COLOR, true, 0);
    }

    /**
     * Confirm that icon was requested from RoundedIconGenerator, and report a
     * supplied bitmap back to the caller.
     *
     * @param bitmap The bitmap to return to the caller (may be null).
     */
    private void verifyRoundedIconRequest(@NonNull GURL url, @Nullable Bitmap bitmap) {
        doReturn(bitmap).when(mIconGenerator).generateIconForUrl(eq(url));
        ShadowLooper.runUiThreadTasks();
        verify(mIconGenerator, times(1)).generateIconForUrl(eq(url));
    }

    /**
     * Confirm the type of icon reported to the caller.
     *
     * @param bitmap The expected bitmap.
     * @param type The expected favicon type.
     */
    private void verifyReturnedIcon(@Nullable Bitmap bitmap, @FaviconType int type) {
        verify(mCallback, times(1)).onFaviconFetchComplete(eq(bitmap), eq(type));
    }

    /**
     * Confirm no unexpected calls were made to any of our data producers or consumers and
     * clear all counters.
     */
    private void verifyNoOtherInteractionsAndClearInteractions() {
        if (mLargeIconBridge != null) {
            verifyNoMoreInteractions(mLargeIconBridge);
            clearInvocations(mLargeIconBridge);
        }
        verifyNoMoreInteractions(mIconGenerator, mCallback);
        clearInvocations(mIconGenerator, mCallback);
    }

    @Test
    public void testIconRetrieval_noLargeIconBridge() {
        // Favicon service does not exist, so we should expect a _single_ call to
        // RoundedIconGenerator.
        mLargeIconBridge = null;
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);
        verifyRoundedIconRequest(NAV_URL, mGeneratedIconBitmap);
        verifyReturnedIcon(mGeneratedIconBitmap, FaviconType.GENERATED);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_largeIconAvailableWithNoBackoff() {
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL, REGULAR_FAVICON_SIZE_PX, mFavIconBitmap);
        verifyReturnedIcon(mFavIconBitmap, FaviconType.REGULAR);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try again, blocking generated icons.
        mFetcher.fetchFaviconWithBackoff(NAV_URL, false, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL, REGULAR_FAVICON_SIZE_PX, mFavIconBitmap);
        verifyReturnedIcon(mFavIconBitmap, FaviconType.REGULAR);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_backOffToSmallIcon() {
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);

        verifyLargeIconBridgeRequest(NAV_URL, REGULAR_FAVICON_SIZE_PX, null);
        verifyLargeIconBridgeRequest(NAV_URL, SMALL_FAVICON_SIZE_PX, mFavIconBitmap);
        verifyReturnedIcon(mFavIconBitmap, FaviconType.SMALL);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try again, blocking generated icons. Observe we go directly for small icon.
        mFetcher.fetchFaviconWithBackoff(NAV_URL, false, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL, SMALL_FAVICON_SIZE_PX, mFavIconBitmap);
        verifyReturnedIcon(mFavIconBitmap, FaviconType.SMALL);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_backOffToGeneratedIcon() {
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);

        verifyLargeIconBridgeRequest(NAV_URL, REGULAR_FAVICON_SIZE_PX, null);
        verifyLargeIconBridgeRequest(NAV_URL, SMALL_FAVICON_SIZE_PX, null);
        verifyRoundedIconRequest(NAV_URL, mGeneratedIconBitmap);
        verifyReturnedIcon(mGeneratedIconBitmap, FaviconType.GENERATED);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try again, observe we go directly for generated icon.
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);
        verifyRoundedIconRequest(NAV_URL, mGeneratedIconBitmap);
        verifyReturnedIcon(mGeneratedIconBitmap, FaviconType.GENERATED);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_backOffWithNoGeneratedIcons() {
        mFetcher.fetchFaviconWithBackoff(NAV_URL, false, mCallback);

        // Expect _no_ calls to icon generator.
        verifyLargeIconBridgeRequest(NAV_URL, REGULAR_FAVICON_SIZE_PX, null);
        verifyLargeIconBridgeRequest(NAV_URL, SMALL_FAVICON_SIZE_PX, null);
        verifyReturnedIcon(null, FaviconType.NONE);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try again, allowing generated icons. Observe we go directly for generated icon.
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);
        verifyRoundedIconRequest(NAV_URL, mGeneratedIconBitmap);
        verifyReturnedIcon(mGeneratedIconBitmap, FaviconType.GENERATED);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try again, rejecting generated icons. Observe we go directly for generated icon.
        mFetcher.fetchFaviconWithBackoff(NAV_URL, false, mCallback);
        verifyReturnedIcon(null, FaviconType.NONE);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_nullBitmapWhenAllMechanismFail() {
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);

        verifyLargeIconBridgeRequest(NAV_URL, REGULAR_FAVICON_SIZE_PX, null);
        verifyLargeIconBridgeRequest(NAV_URL, SMALL_FAVICON_SIZE_PX, null);
        verifyRoundedIconRequest(NAV_URL, null);
        verifyReturnedIcon(null, FaviconType.NONE);
        verifyNoOtherInteractionsAndClearInteractions();

        // Note: re-trying with no generated icons should result with immediate stop.
        mFetcher.fetchFaviconWithBackoff(NAV_URL, false, mCallback);
        verifyReturnedIcon(null, FaviconType.NONE);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_differentUrlsDontCollide() {
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL, REGULAR_FAVICON_SIZE_PX, null);
        verifyLargeIconBridgeRequest(NAV_URL, SMALL_FAVICON_SIZE_PX, null);
        verifyRoundedIconRequest(NAV_URL, mGeneratedIconBitmap);
        verifyReturnedIcon(mGeneratedIconBitmap, FaviconType.GENERATED);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try another URL and confirm we're looking for a regular icon first.
        mFetcher.fetchFaviconWithBackoff(NAV_URL_2, true, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL_2, REGULAR_FAVICON_SIZE_PX, mFavIconBitmap);
        verifyReturnedIcon(mFavIconBitmap, FaviconType.REGULAR);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try the previous URL again and see we already fall back to generated icon.
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);
        verifyRoundedIconRequest(NAV_URL, mGeneratedIconBitmap);
        verifyReturnedIcon(mGeneratedIconBitmap, FaviconType.GENERATED);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_clearingCacheRestartsEntireFlow() {
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL, REGULAR_FAVICON_SIZE_PX, null);
        verifyLargeIconBridgeRequest(NAV_URL, SMALL_FAVICON_SIZE_PX, null);
        verifyRoundedIconRequest(NAV_URL, null);
        verifyReturnedIcon(null, FaviconType.NONE);
        verifyNoOtherInteractionsAndClearInteractions();

        mFetcher.clearCache();

        // Retry. Expect the exact same flow with that same URL.
        mFetcher.fetchFaviconWithBackoff(NAV_URL, true, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL, REGULAR_FAVICON_SIZE_PX, null);
        verifyLargeIconBridgeRequest(NAV_URL, SMALL_FAVICON_SIZE_PX, null);
        verifyRoundedIconRequest(NAV_URL, null);
        verifyReturnedIcon(null, FaviconType.NONE);
        verifyNoOtherInteractionsAndClearInteractions();
    }
}

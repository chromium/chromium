// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

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
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Tests for {@link OmniboxImageSupplier}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public final class OmniboxImageSupplierUnitTest {
    private static final GURL NAV_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL NAV_URL_2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
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
    private @Mock Callback<Bitmap> mCallback;
    private @Mock Profile mProfile;
    private @Px int mFaviconSize;

    @Before
    public void setUp() {
        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeJni);

        var context = ContextUtils.getApplicationContext();
        mFaviconSize = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
        assert mFaviconSize != 0;
        mSupplier = new OmniboxImageSupplier(context);
        mSupplier.setRoundedIconGeneratorForTesting(mIconGenerator);

        doReturn(1L).when(mLargeIconBridgeJni).init();
        doReturn(true)
                .when(mLargeIconBridgeJni)
                .getLargeIconForURL(anyLong(), any(), any(), anyInt(), anyInt(), any());
    }

    /**
     * Confirm that icon of expected size was requested from LargeIconBridge, and report a
     * supplied bitmap back to the caller.
     *
     * @param url the url to expect a lookup for
     * @param bitmap the bitmap to return to the caller (may be null)
     */
    private void verifyLargeIconBridgeRequest(@NonNull GURL url, @Nullable Bitmap bitmap) {
        ShadowLooper.runUiThreadTasks();
        verify(mLargeIconBridgeJni)
                .getLargeIconForURL(anyLong(), eq(mProfile), eq(url), eq(mFaviconSize / 2),
                        eq(mFaviconSize), mIconCallbackCaptor.capture());
        mIconCallbackCaptor.getValue().onLargeIconAvailable(bitmap, FALLBACK_COLOR, true, 0);
    }

    /**
     * Confirm the type of icon reported to the caller.
     *
     * @param bitmap The expected bitmap.
     * @param type The expected favicon type.
     */
    private void verifyReturnedIcon(@Nullable Bitmap bitmap) {
        verify(mCallback, times(1)).onResult(eq(bitmap));
    }

    /**
     * Confirm no unexpected calls were made to any of our data producers or consumers and
     * clear all counters.
     */
    private void verifyNoOtherInteractionsAndClearInteractions() {
        verifyNoMoreInteractions(mLargeIconBridgeJni);
        clearInvocations(mLargeIconBridgeJni);

        verifyNoMoreInteractions(mIconGenerator, mCallback);
        clearInvocations(mIconGenerator, mCallback);
    }

    @Test
    public void fetchFavicon_noLargeIconBridge() {
        // Favicon service does not exist, so we should expect a _single_ call to
        // RoundedIconGenerator.
        mSupplier.fetchFavicon(NAV_URL, mCallback);
        verifyReturnedIcon(null);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void generateFavicon() {
        doReturn(mBitmap1).when(mIconGenerator).generateIconForUrl(NAV_URL);

        mSupplier.generateFavicon(NAV_URL, mCallback);
        ShadowLooper.runUiThreadTasks();

        verify(mIconGenerator, times(1)).generateIconForUrl(NAV_URL);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_largeIconAvailableWithNoBackoff() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni, times(1)).init();
        mSupplier.fetchFavicon(NAV_URL, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL, mBitmap1);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try again, expect icon to be served from LargeIconBridge cache.
        mSupplier.fetchFavicon(NAV_URL, mCallback);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_differentUrlsDontCollide() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni, times(1)).init();

        mSupplier.fetchFavicon(NAV_URL, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL, mBitmap1);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();

        // Try another URL. Expect icon to *not* be served from any caches.
        mSupplier.fetchFavicon(NAV_URL_2, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL_2, mBitmap1);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();
    }

    @Test
    public void testIconRetrieval_clearingCacheRestartsEntireFlow() {
        mSupplier.setProfile(mProfile);
        verify(mLargeIconBridgeJni, times(1)).init();
        mSupplier.fetchFavicon(NAV_URL, mCallback);
        verifyLargeIconBridgeRequest(NAV_URL, mBitmap1);
        verifyReturnedIcon(mBitmap1);
        verifyNoOtherInteractionsAndClearInteractions();

        mSupplier.resetCache();

        // Retry. Expect the exact same flow with that same URL.
        mSupplier.fetchFavicon(NAV_URL, mCallback);
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
    public void resetCache_safeWhenBridgeNotAvailable() {
        mSupplier.resetCache();
    }
}

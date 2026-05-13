// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.lens;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.net.Uri;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.function.Function;

/** Unit tests for {@link LensOverlayCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LensOverlayCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LensOverlayCoordinator.Natives mLensOverlayCoordinatorJniMock;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LensController mLensControllerMock;

    @Captor private ArgumentCaptor<LensIntentParams> mLensIntentParamsCaptor;

    private Activity mActivity;
    private UserDataHost mUserDataHost;

    @Before
    public void setUp() {
        LensOverlayCoordinatorJni.setInstanceForTesting(mLensOverlayCoordinatorJniMock);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mUserDataHost = new UserDataHost();

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        LensController.setInstanceForTesting(mLensControllerMock);
    }

    @After
    public void tearDown() {
        LensController.setInstanceForTesting(null);
        ShareImageFileUtils.setGenerateTemporaryUriFromBitmapHookForTesting(null);
    }

    @Test
    public void start_AlreadyShowing() {
        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);

        // Simulate that the overlay is already showing.
        LensOverlayTabHelper.setOverlayShowing(mTab, true);

        // Attempt to start the overlay again.
        boolean started = coordinator.start(LensOverlayInvocationSource.APP_MENU);

        // Verify that it returns false and does not call into C++.
        assertFalse(started);
        verify(mLensOverlayCoordinatorJniMock, never()).showUI(any(Long.class), any(Integer.class));
    }

    @Test
    public void onCaptureError_ResetsState() {
        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);

        // Simulate that the overlay is currently showing.
        LensOverlayTabHelper.setOverlayShowing(mTab, true);

        // Trigger the asynchronous error callback from C++.
        coordinator.onCaptureError();

        // Verify that the state was reset.
        assertFalse(LensOverlayTabHelper.isOverlayShowing(mTab));
    }

    @Test
    public void onScreenshotCaptured_NullMetricsFallback() {
        GURL testUrl = new GURL("https://example.com");
        when(mTab.getUrl()).thenReturn(testUrl);

        // Simulate metrics collection failure (e.g., lost context).
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(null));

        // Bypass the actual file saving by hooking into ShareImageFileUtils.
        Uri mockUri = Uri.parse("content://mock/screenshot.jpg");
        Function<Bitmap, Uri> mockShareHook = (bitmap) -> mockUri;
        ShareImageFileUtils.setGenerateTemporaryUriFromBitmapHookForTesting(mockShareHook);

        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);
        Bitmap mockBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);

        // Trigger the callback.
        coordinator.onScreenshotCaptured(mockBitmap);

        // Flush UI thread to run the fallback task.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify that startLens was still called (with the raw windowshot).
        verify(mLensControllerMock)
                .startLens(eq(mWindowAndroid), mLensIntentParamsCaptor.capture());
        assertEquals(mockUri, mLensIntentParamsCaptor.getValue().getImageUri());
    }

    @Test
    public void saveCompositedImageAndLaunch_Success() {
        // Setup mock URL and Incognito state for the tab.
        GURL testUrl = new GURL("https://example.com");
        when(mTab.getUrl()).thenReturn(testUrl);
        when(mTab.isIncognito()).thenReturn(false);

        // Bypass the actual file saving by hooking into ShareImageFileUtils.
        Uri mockUri = Uri.parse("content://mock/screenshot.jpg");
        Function<Bitmap, Uri> mockShareHook = (bitmap) -> mockUri;
        ShareImageFileUtils.setGenerateTemporaryUriFromBitmapHookForTesting(mockShareHook);

        // Create the coordinator and trigger the final UI-thread step of the callback chain.
        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);
        Bitmap mockBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        coordinator.saveCompositedImageAndLaunch(mWindowAndroid, mockBitmap);

        // Verify that the LensController was called with the correct parameters.
        verify(mLensControllerMock)
                .startLens(eq(mWindowAndroid), mLensIntentParamsCaptor.capture());

        LensIntentParams capturedParams = mLensIntentParamsCaptor.getValue();
        assertEquals(mockUri, capturedParams.getImageUri());
        assertEquals(testUrl.getSpec(), capturedParams.getPageUrl());
        assertFalse(capturedParams.getIsIncognito());
    }

    @Test
    public void saveCompositedImageAndLaunch_Failure() {
        // Setup mock URL and Incognito state for the tab.
        GURL testUrl = new GURL("https://example.com");
        when(mTab.getUrl()).thenReturn(testUrl);
        when(mTab.isIncognito()).thenReturn(false);

        // Simulate failure in ShareImageFileUtils by returning a null URI.
        Function<Bitmap, Uri> mockShareHook = (bitmap) -> null;
        ShareImageFileUtils.setGenerateTemporaryUriFromBitmapHookForTesting(mockShareHook);

        // Create the coordinator and trigger the final UI-thread step.
        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);

        // Ensure the state starts as showing.
        LensOverlayTabHelper.setOverlayShowing(mTab, true);

        Bitmap mockBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        coordinator.saveCompositedImageAndLaunch(mWindowAndroid, mockBitmap);

        // Verify that the LensController was NEVER called.
        verify(mLensControllerMock, never()).startLens(any(), any());

        // Verify that the showing state was reset to false.
        assertFalse(LensOverlayTabHelper.isOverlayShowing(mTab));
    }
}

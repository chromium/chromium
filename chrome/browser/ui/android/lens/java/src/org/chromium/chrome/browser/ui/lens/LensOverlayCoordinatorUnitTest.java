// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.lens;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.lens.LensSupportStatusHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.function.Function;

/** Unit tests for {@link LensOverlayCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.LENS_OVERLAY_ANDROID)
public class LensOverlayCoordinatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LensOverlayCoordinator.Natives mLensOverlayCoordinatorJniMock;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private LensController mLensControllerMock;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private IdentityManager mIdentityManagerMock;

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
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getIdentityManager(mProfile))
                .thenReturn(mIdentityManagerMock);

        LensController.setInstanceForTesting(mLensControllerMock);
    }

    @After
    public void tearDown() {
        IdentityServicesProvider.setInstanceForTests(null);
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
        verify(mLensOverlayCoordinatorJniMock, never())
                .showUI(any(Long.class), any(Integer.class), any(Boolean.class));
    }

    @Test
    public void start_Success() {
        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);

        // Mock JNI to return true.
        when(mLensOverlayCoordinatorJniMock.showUI(
                        any(Long.class), any(Integer.class), any(Boolean.class)))
                .thenReturn(true);

        // Attempt to start the overlay.
        boolean started = coordinator.start(LensOverlayInvocationSource.APP_MENU);

        // Verify that it returns true.
        assertTrue(started);
        // Verify that the showing state is set to true.
        assertTrue(LensOverlayTabHelper.isOverlayShowing(mTab));
        // Verify JNI call.
        verify(mLensOverlayCoordinatorJniMock, times(1))
                .showUI(any(Long.class), eq(LensOverlayInvocationSource.APP_MENU), eq(false));
    }

    @Test
    public void start_JniFailure() {
        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);

        // Mock JNI to return false (e.g., RenderWidgetHostView was null).
        when(mLensOverlayCoordinatorJniMock.showUI(
                        any(Long.class), any(Integer.class), any(Boolean.class)))
                .thenReturn(false);

        // Attempt to start the overlay.
        boolean started = coordinator.start(LensOverlayInvocationSource.APP_MENU);

        // Verify that it returns false.
        assertFalse(started);
        // Verify that the showing state was reset to false.
        assertFalse(LensOverlayTabHelper.isOverlayShowing(mTab));
    }

    @Test
    public void onScreenshotCaptured_TabDestroyed() {
        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);

        // Simulate that the overlay is currently showing.
        LensOverlayTabHelper.setOverlayShowing(mTab, true);

        // Simulate that the tab's WebContents has been destroyed.
        when(mTab.getWebContents()).thenReturn(null);

        Bitmap mockBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);

        // Trigger the callback.
        coordinator.onScreenshotCaptured(mockBitmap);

        // Verify that the showing state was reset to false.
        assertFalse(LensOverlayTabHelper.isOverlayShowing(mTab));
    }

    @Test
    public void onCaptureError_ResetsState() {
        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);

        // Simulate that the overlay is currently showing.
        LensOverlayTabHelper.setOverlayShowing(mTab, true);

        // Trigger the asynchronous error callback from C++.
        coordinator.onCaptureError();

        // Flush UI thread tasks.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

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

        // Flush UI thread to run any chained tasks (like the fallback task).
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Verify that startLens was still called (with the raw windowshot).
        verify(mLensControllerMock)
                .startLens(eq(mWindowAndroid), mLensIntentParamsCaptor.capture());
        assertEquals(mockUri, mLensIntentParamsCaptor.getValue().getImageUri());
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
        verify(mLensControllerMock, never()).startLens(any(WindowAndroid.class), any());

        // Verify that the showing state was reset to false.
        assertFalse(LensOverlayTabHelper.isOverlayShowing(mTab));
    }

    @Test
    public void saveCompositedImageAndLaunch_WithAccount() {
        // Setup mock URL and account state.
        GURL testUrl = new GURL("https://example.com");
        when(mTab.getUrl()).thenReturn(testUrl);
        when(mTab.isIncognito()).thenReturn(false);
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);

        // Bypass the actual file saving.
        Uri mockUri = Uri.parse("content://mock/screenshot.jpg");
        Function<Bitmap, Uri> mockShareHook = (bitmap) -> mockUri;
        ShareImageFileUtils.setGenerateTemporaryUriFromBitmapHookForTesting(mockShareHook);

        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);
        Bitmap mockBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        coordinator.saveCompositedImageAndLaunch(mWindowAndroid, mockBitmap);

        // Verify that startLens was called with the correct account email.
        verify(mLensControllerMock)
                .startLens(eq(mWindowAndroid), mLensIntentParamsCaptor.capture());
        assertEquals(
                TestAccounts.ACCOUNT1.getEmail(),
                mLensIntentParamsCaptor.getValue().getAccountName());
    }

    @Test
    public void saveCompositedImageAndLaunch_IncognitoNoAccount() {
        // Setup mock URL and incognito state.
        GURL testUrl = new GURL("https://example.com");
        when(mTab.getUrl()).thenReturn(testUrl);
        when(mTab.isIncognito()).thenReturn(true);
        when(mProfile.isOffTheRecord()).thenReturn(true);

        // Even if signed in, incognito should prevent passing the account.
        when(mIdentityManagerMock.getPrimaryAccountInfo()).thenReturn(TestAccounts.ACCOUNT1);

        // Bypass the actual file saving.
        Uri mockUri = Uri.parse("content://mock/screenshot.jpg");
        Function<Bitmap, Uri> mockShareHook = (bitmap) -> mockUri;
        ShareImageFileUtils.setGenerateTemporaryUriFromBitmapHookForTesting(mockShareHook);

        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);
        Bitmap mockBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        coordinator.saveCompositedImageAndLaunch(mWindowAndroid, mockBitmap);

        // Verify that startLens was called with NO account email even if signed in (though here not
        // signed in).
        verify(mLensControllerMock)
                .startLens(eq(mWindowAndroid), mLensIntentParamsCaptor.capture());
        assertEquals(null, mLensIntentParamsCaptor.getValue().getAccountName());
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
        assertTrue(
                "forceUnlockOrientation should be unconditionally true.",
                capturedParams.getForceUnlockOrientation());
        assertFalse(capturedParams.getIsIncognito());
        assertEquals(
                LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM, capturedParams.getLensEntryPoint());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.LENS_OVERLAY_ANDROID
                    + ":"
                    + LensSupportStatusHelper.LENS_OVERLAY_IMPL_TYPE
                    + "/"
                    + LensSupportStatusHelper.LENS_OVERLAY_IMPL_WEBUI)
    public void onScreenshotCaptured_WebUIFlow() {
        // Use a spy to stub the branching methods.
        LensOverlayCoordinator coordinator = spy(LensOverlayCoordinator.getOrCreateForTab(mTab));
        doReturn(true).when(coordinator).startWebUIFlow(any());
        doReturn(true).when(coordinator).startIntentFlow(any());

        // Ensure the state starts as showing.
        LensOverlayTabHelper.setOverlayShowing(mTab, true);

        Bitmap mockBitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);

        // Trigger the callback.
        coordinator.onScreenshotCaptured(mockBitmap);

        // Verify branching.
        verify(coordinator, times(1)).startWebUIFlow(eq(mockBitmap));
        verify(coordinator, never()).startIntentFlow(any());

        // Verify that startLens was NEVER called.
        verify(mLensControllerMock, never()).startLens(any(WindowAndroid.class), any());

        // Verify that the showing state remains true.
        assertTrue(LensOverlayTabHelper.isOverlayShowing(mTab));
    }

    @Test
    public void close_ResetsState() {
        LensOverlayCoordinator coordinator = LensOverlayCoordinator.getOrCreateForTab(mTab);

        // Simulate that the overlay is currently showing.
        LensOverlayTabHelper.setOverlayShowing(mTab, true);

        // Call close().
        coordinator.close();

        // Verify that the state was reset.
        assertFalse(LensOverlayTabHelper.isOverlayShowing(mTab));
    }
}

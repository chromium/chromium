// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.pm.PackageManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;

/** Robolectric tests for {@link BrowserMediaRouterDialogController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BrowserMediaRouterDialogControllerTest {
    private static final long NATIVE_DIALOG_CONTROLLER = 1234L;
    private static final String REMOTING_SOURCE_URN =
            "remote-playback:media-element?source=aHR0cHM6Ly9leGFtcGxlLmNvbS92aWRlby5tcDQ%3D";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowserMediaRouterDialogController.Natives mNativeMock;
    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;

    private BrowserMediaRouterDialogController mController;

    @Before
    public void setUp() {
        BrowserMediaRouterDialogControllerJni.setInstanceForTesting(mNativeMock);
        MediaRouterClient.setInstance(new TestMediaRouterClient());
        doReturn(mWindowAndroid).when(mWebContents).getTopLevelNativeWindow();
        mController =
                BrowserMediaRouterDialogController.create(NATIVE_DIALOG_CONTROLLER, mWebContents);
    }

    @Test
    public void testOpenRouteChooserDialog_requestsPermissionAndCancelsOnDenial() {
        BrowserMediaRouter.setNeedsLocalNetworkPermissionForTesting(true);
        doReturn(true)
                .when(mWindowAndroid)
                .canRequestPermission(BrowserMediaRouter.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK);

        mController.openRouteChooserDialog(new String[] {REMOTING_SOURCE_URN});
        assertTrue(mController.isShowingDialog());

        ArgumentCaptor<PermissionCallback> callbackCaptor =
                ArgumentCaptor.forClass(PermissionCallback.class);
        verify(mWindowAndroid)
                .requestPermissions(
                        eq(
                                new String[] {
                                    BrowserMediaRouter.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK
                                }),
                        callbackCaptor.capture());

        callbackCaptor
                .getValue()
                .onRequestPermissionsResult(
                        new String[] {BrowserMediaRouter.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK},
                        new int[] {PackageManager.PERMISSION_DENIED});

        assertFalse(mController.isShowingDialog());
        assertFalse(BrowserMediaRouter.shouldReportPlaceholderSink());
        verify(mNativeMock).onDialogCancelled(NATIVE_DIALOG_CONTROLLER);
    }

    @Test
    public void testOpenRouteChooserDialog_cannotRequestPermissionCancelsImmediately() {
        BrowserMediaRouter.setNeedsLocalNetworkPermissionForTesting(true);
        doReturn(false)
                .when(mWindowAndroid)
                .canRequestPermission(BrowserMediaRouter.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK);

        mController.openRouteChooserDialog(new String[] {REMOTING_SOURCE_URN});

        assertFalse(mController.isShowingDialog());
        assertFalse(BrowserMediaRouter.shouldReportPlaceholderSink());
        verify(mWindowAndroid, never()).requestPermissions(any(), any());
        verify(mNativeMock).onDialogCancelled(NATIVE_DIALOG_CONTROLLER);
    }

    @Test
    public void testOpenRouteChooserDialog_ignoresStaleCallbackAfterCloseAndReopen() {
        BrowserMediaRouter.setNeedsLocalNetworkPermissionForTesting(true);
        doReturn(true)
                .when(mWindowAndroid)
                .canRequestPermission(BrowserMediaRouter.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK);

        // Start first request, then closeDialog() while permission prompt is in flight.
        mController.openRouteChooserDialog(new String[] {REMOTING_SOURCE_URN});
        assertTrue(mController.isShowingDialog());
        mController.closeDialog();
        assertFalse(mController.isShowingDialog());

        // Start second request before the first request's callback fires.
        mController.openRouteChooserDialog(new String[] {REMOTING_SOURCE_URN});
        assertTrue(mController.isShowingDialog());

        ArgumentCaptor<PermissionCallback> callbackCaptor =
                ArgumentCaptor.forClass(PermissionCallback.class);
        verify(mWindowAndroid, times(2))
                .requestPermissions(
                        eq(
                                new String[] {
                                    BrowserMediaRouter.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK
                                }),
                        callbackCaptor.capture());

        PermissionCallback firstCallback = callbackCaptor.getAllValues().get(0);
        PermissionCallback secondCallback = callbackCaptor.getAllValues().get(1);

        // Stale first callback fires (e.g., cancelled by Android when superseded): must not cancel
        // the active second request or mark local network permission as denied.
        firstCallback.onRequestPermissionsResult(
                new String[] {BrowserMediaRouter.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK},
                new int[] {});
        assertTrue(mController.isShowingDialog());
        assertTrue(BrowserMediaRouter.shouldReportPlaceholderSink());
        verify(mNativeMock, never()).onDialogCancelled(NATIVE_DIALOG_CONTROLLER);

        // Second callback fires with denial: now cancels the active second request.
        secondCallback.onRequestPermissionsResult(
                new String[] {BrowserMediaRouter.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK},
                new int[] {PackageManager.PERMISSION_DENIED});
        assertFalse(mController.isShowingDialog());
        assertFalse(BrowserMediaRouter.shouldReportPlaceholderSink());
        verify(mNativeMock).onDialogCancelled(NATIVE_DIALOG_CONTROLLER);
    }
}

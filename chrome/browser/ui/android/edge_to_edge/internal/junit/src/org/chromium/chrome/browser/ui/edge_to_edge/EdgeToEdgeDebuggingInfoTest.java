// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.graphics.Insets;
import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.EdgeToEdgeDebuggingInfo;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.MissingNavbarInsetsReason;

/** Unit test for {@link EdgeToEdgeDebuggingInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R) // Use an SDK
@EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_DEBUGGING)
public class EdgeToEdgeDebuggingInfoTest {
    private static final Insets NAV_BAR_INSETS = Insets.of(0, 0, 0, 10);
    private static final WindowInsets NAV_BAR_ONLY_INSETS =
            new WindowInsets.Builder()
                    .setInsets(WindowInsets.Type.navigationBars(), NAV_BAR_INSETS)
                    .build();
    private static final WindowInsets NAV_BAR_TAPPABLE_INSETS =
            new WindowInsets.Builder()
                    .setInsets(WindowInsets.Type.navigationBars(), NAV_BAR_INSETS)
                    .setInsets(WindowInsets.Type.tappableElement(), NAV_BAR_INSETS)
                    .build();

    private static final WindowInsets EMPTY_NAV_BAR_INSETS =
            new WindowInsets.Builder()
                    .setInsets(WindowInsets.Type.navigationBars(), Insets.NONE)
                    .setInsets(WindowInsets.Type.tappableElement(), Insets.NONE)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Window mWindow;
    @Mock private View mDecorView;

    private final EdgeToEdgeDebuggingInfo mEdgeToEdgeDebuggingInfo = new EdgeToEdgeDebuggingInfo();
    private final PayloadCallbackHelper<String> mCrashUploadCallback =
            new PayloadCallbackHelper<>();

    @Before
    public void setUp() {
        EdgeToEdgeUtils.setObservedTappableNavigationBarForTesting(false);
        doReturn(mDecorView).when(mWindow).getDecorView();
    }

    @Test
    public void withValidGestureNavBarInsets() {
        setupMockWindowInsets(NAV_BAR_ONLY_INSETS);
        mEdgeToEdgeDebuggingInfo.buildDebugReport(
                mWindow,
                /* windowAndroid= */ null,
                /* hasEdgeToEdgeController= */ true,
                /* isSupportedConfiguration= */ true,
                "callSite",
                mCrashUploadCallback::notifyCalled);

        assertEquals(
                "Configuration is not case of interests.", 0, mCrashUploadCallback.getCallCount());
    }

    @Test
    public void withValidTappableNavBarInsets() {
        // Case where controller is not created, window has consistent tappable nav bar insets.
        setupMockWindowInsets(NAV_BAR_TAPPABLE_INSETS);
        mEdgeToEdgeDebuggingInfo.buildDebugReport(
                mWindow,
                /* windowAndroid= */ null,
                /* hasEdgeToEdgeController= */ false,
                /* isSupportedConfiguration= */ false,
                "callSite",
                mCrashUploadCallback::notifyCalled);

        assertEquals(
                "Configuration is not case of interests.", 0, mCrashUploadCallback.getCallCount());
    }

    @Test
    public void invalidTappableNavBarInsets() {
        setupMockWindowInsets(NAV_BAR_TAPPABLE_INSETS);
        mEdgeToEdgeDebuggingInfo.buildDebugReport(
                mWindow,
                /* windowAndroid= */ null,
                /* hasEdgeToEdgeController= */ true,
                /* isSupportedConfiguration= */ false,
                "callSite",
                mCrashUploadCallback::notifyCalled);

        assertEquals(
                "Configuration is a case of interests.", 1, mCrashUploadCallback.getCallCount());
        assertTrue(mEdgeToEdgeDebuggingInfo.isUsed());
    }

    @Test
    public void emptyTappableNavBarInsets() {
        setupMockWindowInsets(EMPTY_NAV_BAR_INSETS);
        mEdgeToEdgeDebuggingInfo.setMissingNavBarInsetsReason(MissingNavbarInsetsReason.OTHER);
        mEdgeToEdgeDebuggingInfo.buildDebugReport(
                mWindow,
                /* windowAndroid= */ null,
                /* hasEdgeToEdgeController= */ true,
                /* isSupportedConfiguration= */ true,
                "callSite",
                mCrashUploadCallback::notifyCalled);

        assertEquals(
                "Report when nav bar insets are empty at controller creation.",
                1,
                mCrashUploadCallback.getCallCount());
        assertTrue(mEdgeToEdgeDebuggingInfo.isUsed());
    }

    private void setupMockWindowInsets(WindowInsets insets) {
        doReturn(insets).when(mDecorView).getRootWindowInsets();
    }
}

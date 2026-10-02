// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;
import org.chromium.ui.test.util.DeviceRestriction;

import java.lang.ref.WeakReference;

@RunWith(BaseRobolectricTestRunner.class)
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
@Config(sdk = VERSION_CODES.R)
public class EdgeToEdgeControllerCreatorUnitTest {
    private static final int TOP_STATUS_INSET = 150;
    private static final int NAVBAR_INSET = 100;
    private static final int SIDE_GESTURE_INSET = 50;

    private static final Insets STATUS_BAR_INSETS = Insets.of(0, TOP_STATUS_INSET, 0, 0);
    private static final Insets BOTTOM_NAVBAR_INSETS = Insets.of(0, 0, 0, NAVBAR_INSET);
    private static final Insets SYSTEM_BARS_PORTRAIT_INSETS =
            Insets.of(0, TOP_STATUS_INSET, 0, NAVBAR_INSET);
    private static final Insets SYSTEM_GESTURES_WITH_SIDES_INSETS =
            Insets.of(SIDE_GESTURE_INSET, TOP_STATUS_INSET, SIDE_GESTURE_INSET, NAVBAR_INSET);

    private static final WindowInsetsCompat MISSING_SYSTEM_BARS_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), Insets.NONE)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                    .setInsets(WindowInsetsCompat.Type.tappableElement(), Insets.NONE)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), Insets.NONE)
                    .setInsets(
                            WindowInsetsCompat.Type.mandatorySystemGestures(),
                            SYSTEM_BARS_PORTRAIT_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(), SYSTEM_BARS_PORTRAIT_INSETS)
                    .build();

    private static final WindowInsetsCompat TAPPABLE_NAV_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), BOTTOM_NAVBAR_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.tappableElement(), SYSTEM_BARS_PORTRAIT_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), SYSTEM_BARS_PORTRAIT_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.mandatorySystemGestures(),
                            SYSTEM_BARS_PORTRAIT_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(), SYSTEM_BARS_PORTRAIT_INSETS)
                    .build();

    private static final WindowInsetsCompat TAPPABLE_NAV_MISSING_NAVBAR_WINDOW_INSETS =
            new WindowInsetsCompat.Builder(TAPPABLE_NAV_WINDOW_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                    .build();

    private static final WindowInsetsCompat GESTURE_NAV_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), BOTTOM_NAVBAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.tappableElement(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), SYSTEM_BARS_PORTRAIT_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.mandatorySystemGestures(),
                            SYSTEM_BARS_PORTRAIT_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(),
                            SYSTEM_GESTURES_WITH_SIDES_INSETS)
                    .build();

    private static final WindowInsetsCompat GESTURE_NAV_MISSING_NAVBAR_WINDOW_INSETS =
            new WindowInsetsCompat.Builder(GESTURE_NAV_WINDOW_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                    .build();

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private InsetObserver mInsetObserver;

    private final View mView = new View(ContextUtils.getApplicationContext());
    private Activity mActivity;
    private EdgeToEdgeControllerCreator mEdgeToEdgeControllerCreator;
    private final CallbackHelper mInitializeController = new CallbackHelper();

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mEdgeToEdgeControllerCreator =
                new EdgeToEdgeControllerCreator(
                        new WeakReference<>(mActivity),
                        mInsetObserver,
                        mInitializeController::notifyCalled,
                        /* supportsTopInset= */ true);

        mEdgeToEdgeControllerCreator.onApplyWindowInsets(mView, MISSING_SYSTEM_BARS_WINDOW_INSETS);
        assertEquals(
                "The controller should not be initialized, the window insets are still missing"
                        + " system bars.",
                0,
                mInitializeController.getCallCount());
    }

    @Test
    public void gestureNav_initializesControllerWhenAppropriate() {
        mEdgeToEdgeControllerCreator.onApplyWindowInsets(
                mView, GESTURE_NAV_MISSING_NAVBAR_WINDOW_INSETS);
        assertEquals(
                "The controller should not be initialized, the window insets are still missing the"
                        + " navbar.",
                0,
                mInitializeController.getCallCount());

        mEdgeToEdgeControllerCreator.onApplyWindowInsets(mView, GESTURE_NAV_WINDOW_INSETS);
        assertEquals(
                "The controller should be initialized, the window insets now indicate a gesture"
                        + " navigation bar.",
                1,
                mInitializeController.getCallCount());
    }

    @Test
    public void gestureNav_initializesControllerOnlyOnce() {
        mEdgeToEdgeControllerCreator.onApplyWindowInsets(mView, GESTURE_NAV_WINDOW_INSETS);
        assertEquals(
                "The controller should be initialized, the window insets now indicate a gesture"
                        + " navigation bar.",
                1,
                mInitializeController.getCallCount());

        doReturn(true)
                .when(mInsetObserver)
                .hasInsetsConsumer(eq(InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_IMPL));

        mEdgeToEdgeControllerCreator.onApplyWindowInsets(mView, GESTURE_NAV_WINDOW_INSETS);
        assertEquals(
                "A new controller should not be initialized if one has already been created"
                        + " before.",
                1,
                mInitializeController.getCallCount());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET)
    public void tappableNav_neverInitializesController() {
        mEdgeToEdgeControllerCreator.onApplyWindowInsets(
                mView, TAPPABLE_NAV_MISSING_NAVBAR_WINDOW_INSETS);
        assertEquals(
                "The controller should not be initialized, the window insets are still missing the"
                        + " navbar.",
                0,
                mInitializeController.getCallCount());

        mEdgeToEdgeControllerCreator.onApplyWindowInsets(mView, TAPPABLE_NAV_WINDOW_INSETS);
        assertEquals(
                "The controller should not be initialized, the window insets now indicate a"
                        + " tappable navigation bar.",
                0,
                mInitializeController.getCallCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET)
    public void tappableNav_initializesControllerWhenEdgelessTopInsetEnabled() {
        mEdgeToEdgeControllerCreator.onApplyWindowInsets(
                mView, TAPPABLE_NAV_MISSING_NAVBAR_WINDOW_INSETS);
        assertEquals(
                "The controller should be initialized when edgeless top inset is enabled, even with"
                        + " tappable navbar, as long as status bar insets are present.",
                1,
                mInitializeController.getCallCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET)
    public void doNotInitializeEdgeToEdgeController_tappableNav_whenTopInsetNotSupportedByCaller() {
        mEdgeToEdgeControllerCreator.destroy();
        mEdgeToEdgeControllerCreator =
                new EdgeToEdgeControllerCreator(
                        new WeakReference<>(mActivity),
                        mInsetObserver,
                        mInitializeController::notifyCalled,
                        /* supportsTopInset= */ false);

        mEdgeToEdgeControllerCreator.onApplyWindowInsets(mView, TAPPABLE_NAV_WINDOW_INSETS);
        assertEquals(
                "The controller should not be initialized on tappable navbar when top inset is"
                        + " not supported by the caller.",
                0,
                mInitializeController.getCallCount());
    }

    @Test
    @Config(qualifiers = "xlarge")
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET)
    public void tappableNav_onTablet_neverInitializesControllerEvenWhenEdgelessTopInsetEnabled() {
        mEdgeToEdgeControllerCreator.onApplyWindowInsets(mView, TAPPABLE_NAV_WINDOW_INSETS);
        assertEquals(
                "The controller should not be initialized on tablets with tappable navbar, even"
                        + " when edgeless top inset is enabled.",
                0,
                mInitializeController.getCallCount());
    }

    @Test
    @Config(qualifiers = "xlarge")
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TABLET)
    public void bottomChinUnsupported_neverInitializesController() {
        mEdgeToEdgeControllerCreator.onApplyWindowInsets(mView, GESTURE_NAV_WINDOW_INSETS);
        assertEquals(
                "The controller should not be initialized when bottom chin is not supported.",
                0,
                mInitializeController.getCallCount());
    }

    @Test
    public void destroyRemovesInsetConsumption() {
        verify(mInsetObserver, never()).removeInsetsConsumer(any());
        mEdgeToEdgeControllerCreator.destroy();
        verify(mInsetObserver).removeInsetsConsumer(any());
    }
}

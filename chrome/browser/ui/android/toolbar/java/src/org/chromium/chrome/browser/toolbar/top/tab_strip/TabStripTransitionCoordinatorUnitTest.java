// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObscuringHandler.Target;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionHandler;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.DesktopWindowModeState;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

import java.util.concurrent.TimeUnit;

/** Unit test for {@link TabStripTransitionCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w600dp-h800dp", shadows = ShadowLooper.class)
public class TabStripTransitionCoordinatorUnitTest {
    private static final int TEST_TAB_STRIP_HEIGHT = 40;
    private static final int TEST_TOOLBAR_HEIGHT = 56;
    private static final int NOTHING_OBSERVED = -1;
    private static final int LARGE_NORMAL_WINDOW_WIDTH = 413;
    private static final int LARGE_DESKTOP_WINDOW_WIDTH = 285;
    private static final int NARROW_NORMAL_WINDOW_WIDTH = 411;
    private static final int NARROW_DESKTOP_WINDOW_WIDTH = 283;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ControlContainer mControlContainer;
    @Mock private ViewResourceAdapter mViewResourceAdapter;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Captor private ArgumentCaptor<Callback<Resource>> mOnCaptureReadyCallback;

    private TestControlContainerView mSpyControlContainer;
    private TabStripTransitionCoordinator mCoordinator;
    private TestActivity mActivity;
    private final TabObscuringHandler mTabObscuringHandler = new TabObscuringHandler();
    private TestHandler mTestHandler;
    private TestDelegate mDelegate;
    private OneshotSupplierImpl<TabStripTransitionDelegate> mDelegateSupplier;
    private int mReservedTopPadding;

    // Test variables
    private AppHeaderState mAppHeaderState;

    @Before
    public void setup() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);
        mSpyControlContainer = TestControlContainerView.createSpy(mActivity);
        mActivity.setContentView(mSpyControlContainer);
        mReservedTopPadding =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tab_strip_reserved_top_padding);

        // Set the mocks for control container and its view resource adapter.
        doReturn(mSpyControlContainer).when(mControlContainer).getView();
        doReturn(mViewResourceAdapter).when(mControlContainer).getToolbarResourceAdapter();
        doNothing()
                .when(mViewResourceAdapter)
                .addOnResourceReadyCallback(mOnCaptureReadyCallback.capture());
        doAnswer(inv -> triggerCapture()).when(mViewResourceAdapter).triggerBitmapCapture();

        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ false, LARGE_NORMAL_WINDOW_WIDTH);
    }

    @Test
    public void initWithWideWindow() {
        assertEquals(
                "Tab strip height is wrong.",
                TEST_TAB_STRIP_HEIGHT,
                mCoordinator.getTabStripHeight());

        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        assertEquals("Tab strip height is wrong.", 0, mTestHandler.heightRequested);
    }

    @Test
    @Config(qualifiers = "w600dp")
    @CommandLineFlags.Add("tab-strip-height-transition-threshold=700")
    public void initWithWideWindow_CommandlineOverride() {
        assertEquals("Tab strip height requested changing to 0.", 0, mTestHandler.heightRequested);
        assertEquals("Init requested changing to 0.", 0, mCoordinator.getTabStripHeight());

        setDeviceWidthDp(800);
        assertEquals(
                "Changing the window to wide will request for full-size tab strip.",
                TEST_TAB_STRIP_HEIGHT,
                mTestHandler.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void initWithNarrowWindow() {
        assertEquals("Tab strip height requested changing to 0.", 0, mTestHandler.heightRequested);
        assertEquals("Init requested changing to 0.", 0, mCoordinator.getTabStripHeight());

        setDeviceWidthDp(600);
        assertEquals(
                "Changing the window to wide will request for full-size tab strip.",
                TEST_TAB_STRIP_HEIGHT,
                mTestHandler.heightRequested);
    }

    @Test
    public void hideTabStrip() {
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);

        assertTabStripHeightForMargins(0);
        assertObservedHeight(0);
    }

    @Test
    public void hideTabStripWhileUrlBarFocused_Fullscreen() {
        mCoordinator.onUrlFocusChange(true);
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        assertEquals(
                "Height request should be blocked by the url bar focus.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);

        // Url focus animation finished to unblock the transition.
        mCoordinator.onUrlAnimationFinished(false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(
                "Height request should go through after the url bar focus.",
                0,
                mTestHandler.heightRequested);
    }

    @Test
    public void hideTabStripWhileUrlBarFocused_DesktopWindow() {
        // Assume that the tab strip is initially visible in a desktop window.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);

        mCoordinator.onUrlFocusChange(true);
        setDeviceWidthDp(NARROW_DESKTOP_WINDOW_WIDTH);
        assertTrue(
                "Height transition should be blocked.",
                mCoordinator.getHeightTransitionHandlerForTesting().isHeightTransitionBlocked());
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 1f);
    }

    @Test
    public void hideTabStripWhileTabObscured_Fullscreen() {
        TabObscuringHandler.Token token = mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        assertEquals(
                "Height request should be blocked after tab obscured.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);

        // Tab is unobscured to unblock the transition.
        mTabObscuringHandler.unobscure(token);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(
                "Height request should go through after tab unobscured.",
                0,
                mTestHandler.heightRequested);
    }

    @Test
    public void hideTabStripWhileTabObscured_DesktopWindow() {
        // Assume that the tab strip is initially visible in a desktop window.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);

        mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        setDeviceWidthDp(NARROW_DESKTOP_WINDOW_WIDTH);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 1f);
        assertTrue(
                "Height transition should be blocked.",
                mCoordinator.getHeightTransitionHandlerForTesting().isHeightTransitionBlocked());
    }

    @Test
    public void hideTabStripWhileTabAndToolbarObscured() {
        mTabObscuringHandler.obscure(Target.ALL_TABS_AND_TOOLBAR);
        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        assertEquals(
                "Height request should go through when tab and toolbar are obscured.",
                0,
                mTestHandler.heightRequested);
    }

    @Test
    public void hideTabStripDisabledInDesktopWindow() {
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, NARROW_NORMAL_WINDOW_WIDTH);
        assertEquals(
                "Height transition to hide strip is disabled in a small desktop window.",
                TEST_TAB_STRIP_HEIGHT + mReservedTopPadding,
                mTestHandler.heightRequested);
    }

    @Test
    public void hideTabStripBeforeLayout() {
        // Simulate the control container hasn't been measured yet.
        doReturn(0).when(mSpyControlContainer).getWidth();
        doReturn(0).when(mSpyControlContainer).getHeight();

        setDeviceWidthDp(NARROW_NORMAL_WINDOW_WIDTH);
        assertEquals(
                "Height request should be ignored if control container hasn't been measured.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip() {
        settleTransitionDuringInitForNarrowWindow();
        setDeviceWidthDp(600);

        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        assertObservedHeight(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileUrlBarFocused_Fullscreen() {
        settleTransitionDuringInitForNarrowWindow();
        mCoordinator.onUrlFocusChange(true);
        setDeviceWidthDp(600);
        assertEquals(
                "Height request should be blocked by the url bar focus.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);

        // Url focus animation finished to unblock the transition
        mCoordinator.onUrlAnimationFinished(false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(
                "Height request should go through after the url bar focus.",
                TEST_TAB_STRIP_HEIGHT,
                mTestHandler.heightRequested);
    }

    @Test
    public void showTabStripWhileUrlBarFocused_DesktopWindow() {
        // Assume that the tab strip is initially hidden by a fade transition.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, NARROW_DESKTOP_WINDOW_WIDTH);

        // Simulate url bar focus.
        mCoordinator.onUrlFocusChange(true);
        // Increase the width of the strip for it to show.
        setDeviceWidthDp(NARROW_DESKTOP_WINDOW_WIDTH + 100);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 0f);
        assertTrue(
                "Height transition should be blocked.",
                mCoordinator.getHeightTransitionHandlerForTesting().isHeightTransitionBlocked());
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileTabObscured_Fullscreen() {
        settleTransitionDuringInitForNarrowWindow();
        TabObscuringHandler.Token token = mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        setDeviceWidthDp(600);
        assertEquals(
                "Height request should be blocked after tab obscured.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);

        // Tab is unobscured to unblock the transition.
        mTabObscuringHandler.unobscure(token);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(
                "Height request should go through after the tab unobscured.",
                TEST_TAB_STRIP_HEIGHT,
                mTestHandler.heightRequested);
    }

    @Test
    @Config(qualifiers = "w600dp")
    public void showTabStripWhileTabObscured_DesktopWindow() {
        // Assume that the tab strip is initially hidden by a fade transition.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, NARROW_DESKTOP_WINDOW_WIDTH);

        // Simulate obscuring the tab.
        mTabObscuringHandler.obscure(Target.TAB_CONTENT);
        // Increase the width of the strip for it to show.
        setDeviceWidthDp(NARROW_DESKTOP_WINDOW_WIDTH + 100);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 0f);
        assertTrue(
                "Height transition should be blocked.",
                mCoordinator.getHeightTransitionHandlerForTesting().isHeightTransitionBlocked());
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripWhileTabAndToolbarObscured() {
        settleTransitionDuringInitForNarrowWindow();
        mTabObscuringHandler.obscure(Target.ALL_TABS_AND_TOOLBAR);
        setDeviceWidthDp(600);
        assertEquals(
                "Height request should go through if both the tab and toolbar are obscured.",
                TEST_TAB_STRIP_HEIGHT,
                mTestHandler.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip_TokenBeforeLayout() {
        settleTransitionDuringInitForNarrowWindow();
        int token = mCoordinator.requestDeferTabStripTransitionToken();
        setDeviceWidthDp(600);
        assertEquals(
                "Height request should be blocked by the token.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);

        mCoordinator.releaseTabStripToken(token);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(
                "Height request should go through after the token released.",
                TEST_TAB_STRIP_HEIGHT,
                mTestHandler.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip_TokenDuringLayout() {
        settleTransitionDuringInitForNarrowWindow();
        setConfigurationWithNewWidth(600);

        // Layout pass will trigger the delayed task for layout transition.
        simulateLayoutChange(600);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        int token = mCoordinator.requestDeferTabStripTransitionToken();
        assertEquals(
                "Height request should be blocked by the token.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);

        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(
                "Height request should be blocked by the token.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);

        mCoordinator.releaseTabStripToken(token);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(
                "Height request should go through after the token released.",
                TEST_TAB_STRIP_HEIGHT,
                mTestHandler.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStrip_TokenReleaseEarly() {
        settleTransitionDuringInitForNarrowWindow();
        int token = mCoordinator.requestDeferTabStripTransitionToken();
        setConfigurationWithNewWidth(600);
        simulateLayoutChange(600);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        mCoordinator.releaseTabStripToken(token);
        assertEquals(
                "Height request should be blocked by the delayed layout request.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);

        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(
                "Height request should go through after the token released.",
                TEST_TAB_STRIP_HEIGHT,
                mTestHandler.heightRequested);
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void showTabStripBeforeLayout() {
        settleTransitionDuringInitForNarrowWindow();

        // Simulate the control container hasn't been measured yet.
        doReturn(0).when(mSpyControlContainer).getWidth();
        doReturn(0).when(mSpyControlContainer).getHeight();

        setDeviceWidthDp(600);
        assertEquals(
                "Height request should be ignored if control container hasn't been measured.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);
    }

    @Test
    @DisabledTest(message = "crbug.com/424161113")
    public void configurationChangedDuringDelayedTask() {
        setConfigurationWithNewWidth(NARROW_NORMAL_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Tab strip still visible before the delayed transition started.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);

        setDeviceWidthDp(600);
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @DisabledTest(message = "crbug.com/424161113")
    public void destroyDuringDelayedTask() {
        setConfigurationWithNewWidth(NARROW_NORMAL_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        // Tab strip still visible before the delayed transition started.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);

        // Destroy the coordinator so the transition task is canceled.
        mCoordinator.destroy();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    @DisabledTest(message = "crbug.com/424161113")
    public void destroyBeforeCapture() {
        setConfigurationWithNewWidth(NARROW_NORMAL_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        ShadowLooper.idleMainLooper(300, TimeUnit.MILLISECONDS);
        // Tab strip still visible.
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
        // The capture task is scheduled.
        verify(mViewResourceAdapter).addOnResourceReadyCallback(any());

        // Destroy the coordinator so the capture task won't go through.
        mCoordinator.destroy();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertTabStripHeightForMargins(TEST_TAB_STRIP_HEIGHT);
    }

    @Test
    public void enterDesktopWindow_IncreaseHeight() {
        // Simulate a rect update.
        int newHeight = 10 + TEST_TAB_STRIP_HEIGHT;
        Rect appHeaderRect = new Rect(0, 0, 600, newHeight);
        mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, true);
        mCoordinator.onAppHeaderStateChanged(mAppHeaderState);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        assertEquals(
                "Height request should include the top padding.",
                newHeight,
                mTestHandler.heightRequested);

        assertTabStripHeightForMargins(newHeight);
        assertObservedHeight(newHeight);
    }

    @Test
    public void enterDesktopWindow_DecreaseHeight() {
        // Simulate a rect update that has a smaller height.
        int newHeight = TEST_TAB_STRIP_HEIGHT - 10;
        int expectedHeight = mReservedTopPadding + TEST_TAB_STRIP_HEIGHT;
        Rect appHeaderRect = new Rect(0, 0, 600, newHeight);
        mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, true);
        mCoordinator.onAppHeaderStateChanged(mAppHeaderState);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        assertEquals(
                "When new height is less than height with reserved padding, use that instead.",
                expectedHeight,
                mTestHandler.heightRequested);

        assertTabStripHeightForMargins(expectedHeight);
        assertObservedHeight(expectedHeight);
    }

    @Test
    public void enterDesktopWindow_DecreaseWidth() {
        // Simulate a rect update that has a smaller width.
        int newHeight = TEST_TAB_STRIP_HEIGHT + mReservedTopPadding;
        Rect appHeaderRect = new Rect(0, 0, NARROW_DESKTOP_WINDOW_WIDTH, newHeight);
        mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, true);
        mCoordinator.onAppHeaderStateChanged(mAppHeaderState);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        assertEquals(
                "Narrow width does not trigger tab strip height transition.",
                newHeight,
                mTestHandler.heightRequested);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 1f);
    }

    @Test
    public void enterDesktopWindow_NarrowInitialWidth() {
        // Create the transition coordinator again for a narrow width desktop window.
        int newHeight = TEST_TAB_STRIP_HEIGHT + mReservedTopPadding;
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, NARROW_DESKTOP_WINDOW_WIDTH);

        assertEquals(
                "Tab strip height transition was not triggered for window with narrow width.",
                newHeight,
                mTestHandler.heightRequested);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 1f);
    }

    @Test
    public void enterDesktopWindow_WideInitialWidth() {
        // Create the transition coordinator again for a large desktop window.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);
        verifyFadeTransitionState(/* expectedScrimOpacity= */ 0f);
    }

    @Test
    public void enterDesktopWindow_WithoutControlContainerLayout() {
        // Set the height as if the first measure pass hasn't happened yet.
        doReturn(0).when(mSpyControlContainer).getHeight();
        doReturn(0).when(mSpyControlContainer).getWidth();

        // Create the transition coordinator for a desktop window.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);

        assertEquals(
                "Height request should be ignored if control container hasn't been measured.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);
    }

    @Test
    public void recordHistogramWindowResize_LayoutChangeInDesktopWindow() {
        // Simulate desktop windowing mode.
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.WindowResize.DesktopWindowModeState",
                        DesktopWindowModeState.ACTIVE);
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);
        // Histogram should be emitted only when the strip size is changing across multiple layout
        // changes.
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        watcher.assertExpected();
    }

    @Test
    public void recordHistogramWindowResize_LayoutChangeNotInDesktopWindow_SupportedDevice() {
        // Simulate non-desktop windowing mode on a supported device.
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ false, LARGE_NORMAL_WINDOW_WIDTH);
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.WindowResize.DesktopWindowModeState",
                        DesktopWindowModeState.INACTIVE);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        watcher.assertExpected();
    }

    @Test
    public void recordHistogramWindowResize_LayoutChangeNotInDesktopWindow_UnsupportedDevice() {
        // Create the transition coordinator with an initial null value of
        // DesktopWindowStateManager that is representative of an unsupported device.
        mDesktopWindowStateManager = null;
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ false, LARGE_NORMAL_WINDOW_WIDTH);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DynamicTopChrome.WindowResize.DesktopWindowModeState",
                        DesktopWindowModeState.UNAVAILABLE);
        simulateLayoutChange(NARROW_NORMAL_WINDOW_WIDTH);
        watcher.assertExpected();
    }

    // Tests for transitions initiated during desktop windowing mode changes.

    @Test
    public void smallFullscreenWindowToSmallDesktopWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ true,
                /* tokenInUse= */ false);
    }

    @Test
    public void smallFullscreenWindowToLargeDesktopWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ false);
    }

    @Test
    public void largeFullscreenWindowToSmallDesktopWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ true,
                /* tokenInUse= */ false);
    }

    @Test
    public void largeFullscreenWindowToLargeDesktopWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ false);
    }

    @Test
    public void smallFullscreenWindowToSmallDesktopWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ true,
                /* tokenInUse= */ true);
    }

    @Test
    public void smallFullscreenWindowToLargeDesktopWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ true);
    }

    @Test
    public void largeFullscreenWindowToSmallDesktopWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ true,
                /* tokenInUse= */ true);
    }

    @Test
    public void largeFullscreenWindowToLargeDesktopWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ true);
    }

    @Test
    public void smallDesktopWindowToSmallFullscreenWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ true,
                /* tokenInUse= */ false);
    }

    @Test
    public void smallDesktopWindowToLargeFullscreenWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ false);
    }

    @Test
    public void largeDesktopWindowToSmallFullscreenWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ true,
                /* tokenInUse= */ false);
    }

    @Test
    public void largeDesktopWindowToLargeFullscreenWindow_TokenNotInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ false);
    }

    @Test
    public void smallDesktopWindowToSmallFullscreenWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ true,
                /* tokenInUse= */ true);
    }

    @Test
    public void smallDesktopWindowToLargeFullscreenWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ true);
    }

    @Test
    public void largeDesktopWindowToSmallFullscreenWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ true,
                /* tokenInUse= */ true);
    }

    @Test
    public void largeDesktopWindowToLargeFullscreenWindow_TokenInUse() {
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ false,
                /* smallSourceWindow= */ false,
                /* smallDestinationWindow= */ false,
                /* tokenInUse= */ true);
    }

    @Test
    public void multipleWindowingModeSwitchesWithoutWidthChange() {
        int count = mDelegate.fadeTransitionCallback.getCallCount();

        // Open a small fullscreen window and switch to a small desktop window.
        doTestDesktopWindowModeChanged(
                /* enterDesktopWindow= */ true,
                /* smallSourceWindow= */ true,
                /* smallDestinationWindow= */ true,
                /* tokenInUse= */ false);

        // Switch to a fullscreen window of the same width.
        simulateAppHeaderStateChanged(NARROW_DESKTOP_WINDOW_WIDTH, false);
        simulateLayoutChange(NARROW_DESKTOP_WINDOW_WIDTH);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        assertEquals("Height is not as expected.", 0, mTestHandler.heightRequested);
        assertTrue("Scrim overlay is not applied as expected.", mDelegate.applyScrimOverlay);

        // Switch to a desktop window of the same width.
        simulateAppHeaderStateChanged(NARROW_DESKTOP_WINDOW_WIDTH, true);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        // Fade transition should be requested once initially while switching to a small desktop
        // window, and again when switching back to a small window of the same width after switching
        // out of desktop windowing mode to a window of the same width.
        assertEquals(
                "Fade transition was not requested twice.",
                count + 2,
                mDelegate.fadeTransitionCallback.getCallCount());
        verifyFadeTransitionState(1f);
    }

    @Test
    public void transitionUpdatesTopPaddingOnAppThemeChange() {
        // Simulate re-instantiation of the coordinator when the control container hasn't been
        // measured yet, that happens on an app theme change.
        doReturn(0).when(mSpyControlContainer).getHeight();
        setUpTabStripTransitionCoordinator(
                /* isInDesktopWindow= */ true, LARGE_DESKTOP_WINDOW_WIDTH);
        assertEquals(
                "Height transition to update top padding should not be requested when control"
                        + " container has not been measured.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);

        // Simulate a layout pass where the control container is measured, upon navigation back to
        // the active tab from theme settings.
        doReturn(TEST_TOOLBAR_HEIGHT + TEST_TAB_STRIP_HEIGHT)
                .when(mSpyControlContainer)
                .getHeight();
        simulateLayoutChange(LARGE_DESKTOP_WINDOW_WIDTH);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(
                "Height transition should update the strip top padding.",
                TEST_TAB_STRIP_HEIGHT + mReservedTopPadding,
                mTestHandler.heightRequested);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.LOCK_TOP_CONTROLS_ON_LARGE_TABLETS_V2
                + ":adjust_tab_strip_on_startup/true"
    })
    @Config(qualifiers = "sw720dp")
    public void adjustOnStartup_OnDesktopWindowUpdate_Wide() {
        // Deliberately having the resource adapter no-op to test startup flow.
        doNothing().when(mViewResourceAdapter).triggerBitmapCapture();

        // Set up the coordinator. Launching Chrome in DW mode, verify that the coordinator request
        // goes through without waiting for the resource adapter.
        setUpTabStripTransitionCoordinator(
                true, LARGE_NORMAL_WINDOW_WIDTH, /* initDelegate= */ false);
        assertEquals(
                "Height request should go through without waiting for the capture.",
                TEST_TAB_STRIP_HEIGHT + mReservedTopPadding,
                mTestHandler.heightRequested);
        assertFalse(
                "Height request should go through without waiting for the capture.",
                mTestHandler.applyScrimOverlay);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.LOCK_TOP_CONTROLS_ON_LARGE_TABLETS_V2
                + ":adjust_tab_strip_on_startup/false"
    })
    public void adjustOnStartup_OnDesktopWindowUpdate_FeatureDisabled() {
        // Deliberately having the resource adapter no-op to test startup flow.
        doNothing().when(mViewResourceAdapter).triggerBitmapCapture();

        // Set up the coordinator. Launching Chrome in DW mode, verify that the coordinator request
        // does not go through.
        setUpTabStripTransitionCoordinator(
                true, LARGE_NORMAL_WINDOW_WIDTH, /* initDelegate= */ false);
        assertEquals(
                "Height request should not go through when feature disabled.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.LOCK_TOP_CONTROLS_ON_LARGE_TABLETS_V2
                + ":adjust_tab_strip_on_startup/true"
    })
    public void adjustOnStartup_NotInDesktopWindow() {
        // Deliberately having the resource adapter no-op to test startup flow.
        doNothing().when(mViewResourceAdapter).triggerBitmapCapture();

        // Set up the coordinator. Launching Chrome in non-DW mode, verify that the coordinator
        // request does not go through.
        setUpTabStripTransitionCoordinator(
                false, LARGE_NORMAL_WINDOW_WIDTH, /* initDelegate= */ false);
        assertEquals(
                "Height request should not go through when not in desktop windowing mode.",
                NOTHING_OBSERVED,
                mTestHandler.heightRequested);
    }

    private void doTestDesktopWindowModeChanged(
            boolean enterDesktopWindow,
            boolean smallSourceWindow,
            boolean smallDestinationWindow,
            boolean tokenInUse) {
        // Setup widths based on test requirement.
        int sourceWidth;
        int destinationWidth;
        if (enterDesktopWindow) {
            sourceWidth =
                    smallSourceWindow ? NARROW_NORMAL_WINDOW_WIDTH : LARGE_NORMAL_WINDOW_WIDTH;
            destinationWidth =
                    smallDestinationWindow
                            ? NARROW_DESKTOP_WINDOW_WIDTH
                            : LARGE_DESKTOP_WINDOW_WIDTH;
        } else {
            sourceWidth =
                    smallSourceWindow ? NARROW_DESKTOP_WINDOW_WIDTH : LARGE_DESKTOP_WINDOW_WIDTH;
            destinationWidth =
                    smallDestinationWindow ? NARROW_NORMAL_WINDOW_WIDTH : LARGE_NORMAL_WINDOW_WIDTH;
        }

        // Initialize the coordinator with the start state.
        setUpTabStripTransitionCoordinator(!enterDesktopWindow, sourceWidth);

        // If the test requires blocking a height transition by acquiring a token, simulate this
        // scenario.
        if (tokenInUse) {
            mCoordinator.onUrlFocusChange(true);
        }

        // Simulate switching desktop windowing mode.
        simulateAppHeaderStateChanged(destinationWidth, enterDesktopWindow);
        simulateLayoutChange(destinationWidth);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        // Verify the last height request made to the transition delegate.
        int expectedHeight;
        int expectedHeightAfterTokenRelease;
        boolean expectedApplyScrimOverlay = false;
        if (enterDesktopWindow) {
            expectedHeightAfterTokenRelease = TEST_TAB_STRIP_HEIGHT + mReservedTopPadding;
            // Height should be force-updated in a desktop window since its goal is to only update
            // strip top padding and not visibility.
            expectedHeight = expectedHeightAfterTokenRelease;
        } else {
            // Height will not be updated while exiting desktop windowing mode if the transition is
            // blocked.
            expectedHeightAfterTokenRelease = smallDestinationWindow ? 0 : TEST_TAB_STRIP_HEIGHT;
            expectedHeight =
                    tokenInUse
                            ? TEST_TAB_STRIP_HEIGHT + mReservedTopPadding
                            : expectedHeightAfterTokenRelease;
            expectedApplyScrimOverlay = true;
        }
        assertEquals("Height is not as expected.", expectedHeight, mTestHandler.heightRequested);

        // Verify the strip scrim opacity request made to the transition delegate.
        boolean forceFadeInTransition = !enterDesktopWindow && tokenInUse && smallSourceWindow;
        if (enterDesktopWindow || forceFadeInTransition) {
            // While exiting desktop windowing mode, scrim opacity will be updated via a fade
            // transition only when switching from a window with an invisible strip and when the
            // height transition is blocked. In all other cases, the height transition will be
            // responsible for updating the scrim opacity while exiting a desktop window.
            float expectedScrimOpacity =
                    forceFadeInTransition ? 0f : (smallDestinationWindow ? 1f : 0f);
            verifyFadeTransitionState(expectedScrimOpacity);
        }

        // If testing a scenario with tokens in use, unblock the height transition and verify that
        // the desired height request was made.
        if (tokenInUse) {
            mCoordinator.onUrlAnimationFinished(false);
            RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
            assertEquals(
                    "Height request should go through after the token is released.",
                    expectedHeightAfterTokenRelease,
                    mTestHandler.heightRequested);
        }

        assertEquals(
                "Scrim overlay is not applied as expected.",
                expectedApplyScrimOverlay,
                mDelegate.applyScrimOverlay);
    }

    private void setUpTabStripTransitionCoordinator(boolean isInDesktopWindow, int windowWidth) {
        setUpTabStripTransitionCoordinator(
                isInDesktopWindow, windowWidth, /* initDelegate= */ true);
    }

    private void setUpTabStripTransitionCoordinator(
            boolean isInDesktopWindow, int windowWidth, boolean initDelegate) {
        if (mDesktopWindowStateManager != null) {
            int stripHeight = TEST_TAB_STRIP_HEIGHT + (isInDesktopWindow ? mReservedTopPadding : 0);
            var appHeaderRect =
                    isInDesktopWindow ? new Rect(0, 0, windowWidth, stripHeight) : new Rect();
            mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, isInDesktopWindow);
            doAnswer((arg) -> mAppHeaderState).when(mDesktopWindowStateManager).getAppHeaderState();
        }

        mDelegate = new TestDelegate();
        mDelegateSupplier = new OneshotSupplierImpl<>();
        if (initDelegate) {
            mDelegateSupplier.set(mDelegate);
        }
        mTestHandler = new TestHandler();
        mCoordinator =
                new TabStripTransitionCoordinator(
                        mControlContainer,
                        TEST_TAB_STRIP_HEIGHT,
                        mTabObscuringHandler,
                        mDesktopWindowStateManager,
                        mDelegateSupplier,
                        mTestHandler);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
    }

    private void setDeviceWidthDp(int widthDp) {
        Configuration configuration = setConfigurationWithNewWidth(widthDp);
        simulateConfigurationChanged(configuration);
        simulateAppHeaderStateChanged(widthDp, mAppHeaderState.isInDesktopWindow());
        simulateLayoutChange(widthDp);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
    }

    private Configuration setConfigurationWithNewWidth(int widthDp) {
        Resources res = mActivity.getResources();
        DisplayMetrics displayMetrics = res.getDisplayMetrics();
        displayMetrics.widthPixels = (int) (displayMetrics.density * widthDp);

        Configuration configuration = res.getConfiguration();
        configuration.screenWidthDp = widthDp;
        mActivity.createConfigurationContext(configuration);
        return configuration;
    }

    private void assertTabStripHeightForMargins(int tabStripHeight) {
        assertEquals(tabStripHeight, mDelegate.heightChanged);
    }

    private void assertObservedHeight(int tabStripHeight) {
        assertEquals(
                "#getHeight has a different value.",
                tabStripHeight,
                mCoordinator.getTabStripHeight());

        assertEquals(
                "Delegate#onHeightChanged received a different value.",
                tabStripHeight,
                mDelegate.heightChanged);
    }

    private Void triggerCapture() {
        var callback = mOnCaptureReadyCallback.getValue();
        assertNotNull("Capture callback is null.", callback);
        callback.onResult(null);
        return null;
    }

    // For test cases init with narrow width, the initialization will create an transition request.
    private void settleTransitionDuringInitForNarrowWindow() {
        setUpTabStripTransitionCoordinator(false, NARROW_NORMAL_WINDOW_WIDTH);
        mTestHandler.reset();
        mDelegate.reset();
    }

    private void simulateLayoutChange(int width) {
        assertNotNull(mSpyControlContainer.onLayoutChangeListener);
        mSpyControlContainer.onLayoutChangeListener.onLayoutChange(
                mSpyControlContainer,
                /* left= */ 0,
                /* top= */ 0,
                /* right= */ width,
                /* bottom= */ 0,
                0,
                0,
                0,
                0);
    }

    private void simulateConfigurationChanged(Configuration newConfig) {
        mCoordinator.onConfigurationChanged(newConfig != null ? newConfig : new Configuration());
    }

    private void simulateAppHeaderStateChanged(int width, boolean isInDesktopWindow) {
        int stripHeight = TEST_TAB_STRIP_HEIGHT + (isInDesktopWindow ? mReservedTopPadding : 0);
        var appHeaderRect = isInDesktopWindow ? new Rect(0, 0, width, stripHeight) : new Rect();
        mAppHeaderState = new AppHeaderState(appHeaderRect, appHeaderRect, isInDesktopWindow);
        mCoordinator.onAppHeaderStateChanged(mAppHeaderState);
    }

    private void verifyFadeTransitionState(float expectedScrimOpacity) {
        assertEquals(
                "Fade transition end opacity is incorrect.",
                expectedScrimOpacity,
                mDelegate.scrimOpacityRequested,
                0f);
    }

    // Due to the complexity to use the real views for top toolbar in robolectric tests, use view
    // mocks for the sake of unit tests.
    static class TestControlContainerView extends FrameLayout {
        public View toolbarLayout;
        @Nullable public View.OnLayoutChangeListener onLayoutChangeListener;

        static TestControlContainerView createSpy(Context context) {
            TestControlContainerView controlContainer =
                    spy(new TestControlContainerView(context, null));
            doReturn(controlContainer.toolbarLayout)
                    .when(controlContainer)
                    .findViewById(R.id.toolbar);
            doAnswer(args -> context.getResources().getDisplayMetrics().widthPixels)
                    .when(controlContainer)
                    .getWidth();
            // Set a test height for the control container as if it's already being measured.
            doReturn(TEST_TOOLBAR_HEIGHT + TEST_TAB_STRIP_HEIGHT)
                    .when(controlContainer)
                    .getHeight();
            doAnswer(
                            args -> {
                                controlContainer.onLayoutChangeListener = args.getArgument(0);
                                return null;
                            })
                    .when(controlContainer)
                    .addOnLayoutChangeListener(any());

            return controlContainer;
        }

        public TestControlContainerView(Context context, @Nullable AttributeSet attrs) {
            super(context, attrs);

            toolbarLayout = spy(new View(context, attrs));
            when(toolbarLayout.getHeight()).thenReturn(TEST_TOOLBAR_HEIGHT);
        }
    }

    class TestHandler implements TabStripTransitionHandler {
        public int heightRequested = NOTHING_OBSERVED;
        public int controlContainerHeight = NOTHING_OBSERVED;
        public boolean applyScrimOverlay;
        public int topPadding;

        @Override
        public void onTransitionRequested(
                int newHeight,
                int topPadding,
                boolean applyScrimOverlay,
                Runnable transitionStartedCallback) {
            this.heightRequested = newHeight;
            this.applyScrimOverlay = applyScrimOverlay;
            if (transitionStartedCallback != null) {
                transitionStartedCallback.run();
            }
            if (mDelegate != null) {
                mDelegate.onHeightChanged(newHeight, topPadding, applyScrimOverlay);
                mDelegate.onHeightTransitionFinished(true);
            }
        }

        void reset() {
            heightRequested = NOTHING_OBSERVED;
            controlContainerHeight = NOTHING_OBSERVED;
        }
    }

    static class TestDelegate implements TabStripTransitionDelegate {
        public int heightChanged = NOTHING_OBSERVED;
        public boolean heightTransitionFinished;
        public float scrimOpacityRequested = NOTHING_OBSERVED;
        public boolean applyScrimOverlay;
        public final CallbackHelper fadeTransitionCallback = new CallbackHelper();
        public boolean hiddenByFade;

        void reset() {
            heightChanged = NOTHING_OBSERVED;
            heightTransitionFinished = false;
            scrimOpacityRequested = NOTHING_OBSERVED;
            applyScrimOverlay = false;
        }

        @Override
        public void onHeightChanged(int newHeight, int topPadding, boolean applyScrimOverlay) {
            heightChanged = newHeight;
            this.applyScrimOverlay = applyScrimOverlay;
        }

        @Override
        public void onHeightTransitionFinished(boolean success) {
            heightTransitionFinished = true;
        }

        @Override
        public void onFadeTransitionRequested(float newOpacity, int durationMs) {
            scrimOpacityRequested = newOpacity;
            hiddenByFade = (newOpacity != 0f);
            fadeTransitionCallback.notifyCalled();
        }

        @Override
        public boolean isHiddenByFadeTransition() {
            return hiddenByFade;
        }

        @Override
        public int getFadeTransitionThresholdDp() {
            return NARROW_DESKTOP_WINDOW_WIDTH + 1;
        }
    }
}

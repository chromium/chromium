// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator.INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
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
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinatorUnitTest.ShadowDisplayUtil;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.DesktopWindowHeuristicResult;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.WindowingMode;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.ui.CaptionBarInsetsRectProvider;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetsRectProvider;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.display.DisplayUtil;

import java.util.List;

/** Unit test for {@link AppHeaderCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30, shadows = ShadowDisplayUtil.class)
@LooperMode(Mode.PAUSED)
public class AppHeaderCoordinatorUnitTest {
    @Implements(DisplayUtil.class)
    static class ShadowDisplayUtil {
        private static boolean sIsOnDefaultDisplay;

        private static void setOnDefaultDisplay(boolean isOnDefaultDisplay) {
            sIsOnDefaultDisplay = isOnDefaultDisplay;
        }

        @Implementation
        public static boolean isContextInDefaultDisplay(Context context) {
            return sIsOnDefaultDisplay;
        }
    }

    private static final int WINDOW_WIDTH = 600;
    private static final int WINDOW_HEIGHT = 800;
    private static final Rect WINDOW_RECT = new Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    private static final int LEFT_BLOCK = 10;
    private static final int RIGHT_BLOCK = 20;
    private static final int HEADER_HEIGHT = 30;
    private static final Rect WIDEST_UNOCCLUDED_RECT =
            new Rect(LEFT_BLOCK, 0, WINDOW_WIDTH - RIGHT_BLOCK, HEADER_HEIGHT);
    private static final int KEYBOARD_INSET = 736;
    private static final int NAV_BAR_INSET = 128;
    private static final int UNSPECIFIED_INSET = -1;
    private static final int APPEARANCE_LIGHT_CAPTION_BARS = 1 << 8;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisDelegate;
    @Mock private InsetObserver mInsetObserver;
    @Mock private CaptionBarInsetsRectProvider mInsetsRectProvider;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private DesktopWindowStateManager.AppHeaderObserver mObserver;
    @Captor private ArgumentCaptor<InsetsRectProvider.Observer> mInsetRectObserverCaptor;

    private AppHeaderCoordinator mAppHeaderCoordinator;
    private Activity mSpyActivity;
    private View mSpyRootView;
    private WindowInsetsCompat mLastSeenRawWindowInsets = new WindowInsetsCompat(null);
    private Bundle mSavedInstanceStateBundle;
    private EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;

    @Before
    public void setup() {
        ShadowDisplayUtil.setOnDefaultDisplay(true);
        mActivityScenarioRule.getScenario().onActivity(activity -> mSpyActivity = spy(activity));
        mEdgeToEdgeStateProvider = new EdgeToEdgeStateProvider(mSpyActivity.getWindow());
        mSpyRootView = spy(mSpyActivity.getWindow().getDecorView());
        AppHeaderCoordinator.setInsetsRectProviderForTesting(mInsetsRectProvider);
        AppHeaderUtils.resetHeaderCustomizationDisallowedOnExternalDisplayForOemForTesting();
        doAnswer(inv -> mLastSeenRawWindowInsets).when(mInsetObserver).getLastRawWindowInsets();
        setupWithNoCaptionInsets();
        mSavedInstanceStateBundle = new Bundle();
        initAppHeaderCoordinator();
    }

    @Test
    public void notEnabledWithNoTopInsets() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult3",
                        DesktopWindowHeuristicResult.CAPTION_BAR_TOP_INSETS_ABSENT);
        // Bottom insets with height = 30
        Insets bottomInsets = Insets.of(0, 0, 0, 30);
        // Left block: 10, right block: 20
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, WINDOW_HEIGHT - 30, LEFT_BLOCK, WINDOW_HEIGHT),
                        new Rect(
                                WINDOW_WIDTH - RIGHT_BLOCK,
                                WINDOW_HEIGHT - 30,
                                LEFT_BLOCK,
                                WINDOW_HEIGHT));
        Rect widestUnOccludedRect =
                new Rect(LEFT_BLOCK, WINDOW_HEIGHT - 30, WINDOW_WIDTH - RIGHT_BLOCK, WINDOW_HEIGHT);
        setupInsetsRectProvider(bottomInsets, blockedRects, widestUnOccludedRect, WINDOW_RECT);
        notifyInsetsRectObserver();

        verifyDesktopWindowingDisabled(
                /* error= */ "Desktop Windowing not enabled for bottom insets.");
        watcher.assertExpected();
    }

    @Test
    public void notEnabledWithBoundingRectsWithPartialHeight() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult3",
                        DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT);
        // Bottom insets with height = 30
        Insets insets = Insets.of(0, 30, 0, 0);
        // Left block: 10, right block: 20. Bounding rect is not at the full height.
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, 0, LEFT_BLOCK, HEADER_HEIGHT - 10),
                        new Rect(WINDOW_WIDTH - RIGHT_BLOCK, 0, WINDOW_WIDTH, HEADER_HEIGHT - 10));
        Rect widestUnoccludedRect =
                new Rect(LEFT_BLOCK, 0, WINDOW_WIDTH - RIGHT_BLOCK, HEADER_HEIGHT - 10);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, WINDOW_RECT);
        notifyInsetsRectObserver();

        verifyDesktopWindowingDisabled(
                /* error= */ "Desktop Windowing enabled for widestUnOccludedRect with less height"
                        + " than the insets.");
        watcher.assertExpected();
    }

    @Test
    public void notEnabledWhenWidestUnoccludedRectIsEmpty() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult3",
                        DesktopWindowHeuristicResult.WIDEST_UNOCCLUDED_RECT_EMPTY);
        setupInsetsRectProvider(Insets.NONE, List.of(), new Rect(), WINDOW_RECT);
        notifyInsetsRectObserver();

        verifyDesktopWindowingDisabled(
                /* error= */ "Desktop windowing should not be enabled when widest unoccluded rect"
                        + " is empty.");
        watcher.assertExpected();
    }

    @Test
    public void notEnabledOnExternalDisplayWhenDisallowed() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult3",
                        DesktopWindowHeuristicResult.DISALLOWED_ON_EXTERNAL_DISPLAY);
        ShadowDisplayUtil.setOnDefaultDisplay(false);
        updateFeatureParams(/* enableOnExternalDisplay= */ false, /* oemDenylist= */ "");
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        verifyDesktopWindowingDisabled(
                /* error= */ "Desktop windowing should not be enabled on an external display when"
                        + " it is disallowed.");
        watcher.assertExpected();
    }

    @Test
    public void notEnabledOnExternalDisplayForDenylistedOem() {
        ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "samsung");
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult3",
                        DesktopWindowHeuristicResult.DISALLOWED_ON_EXTERNAL_DISPLAY);
        // Assume external display support is enabled but denylisted for "samsung".
        ShadowDisplayUtil.setOnDefaultDisplay(false);
        updateFeatureParams(/* enableOnExternalDisplay= */ true, /* oemDenylist= */ "samsung");
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        verifyDesktopWindowingDisabled(
                /* error= */ "Desktop windowing should not be enabled on an external display when"
                        + " it is denylisted for the OEM.");
        watcher.assertExpected();
    }

    @Test
    public void enabledOnExternalDisplayForNonDenylistedOem() {
        ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "lenovo");
        // Assume external display support is enabled but denylisted for "samsung".
        ShadowDisplayUtil.setOnDefaultDisplay(false);
        updateFeatureParams(/* enableOnExternalDisplay= */ true, /* oemDenylist= */ "samsung");
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        verifyDesktopWindowingEnabled();
    }

    @Test
    public void enabledOnExternalDisplayWhenAllowed() {
        ShadowDisplayUtil.setOnDefaultDisplay(false);
        updateFeatureParams(/* enableOnExternalDisplay= */ true, /* oemDenylist= */ "");
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        verifyDesktopWindowingEnabled();
    }

    @Test
    public void enableDesktopWindowing() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.DesktopWindowHeuristicResult3",
                                DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW,
                                1)
                        .expectIntRecordTimes(
                                "Android.MultiWindowMode.Configuration",
                                WindowingMode.DESKTOP_WINDOW,
                                1)
                        .build();
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        verifyDesktopWindowingEnabled();

        var expectedState = new AppHeaderState(WINDOW_RECT, WIDEST_UNOCCLUDED_RECT, true);
        assertEquals(
                "AppHeaderState is different.",
                expectedState,
                mAppHeaderCoordinator.getAppHeaderState());
        verify(mObserver).onAppHeaderStateChanged(eq(expectedState));
        watcher.assertExpected();
    }

    @Test
    public void desktopWindowHeuristicResultHistogramNotRecordedWithSameValues() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes("Android.DesktopWindowHeuristicResult3", 1)
                        .build();
        setupWithLeftAndRightBoundingRect();
        // Override the last seen raw insets so there's a bottom nav bar insets.
        mLastSeenRawWindowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, 10))
                        .build();

        // Simulate multiple rect updates that will trigger the heuristic checks for desktop
        // windowing mode.
        notifyInsetsRectObserver();
        notifyInsetsRectObserver();

        // Histogram should be emitted just once.
        watcher.assertExpected();
    }

    @Test
    public void changeBoundingRects() {
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Assume the window size changed.
        // Top insets with height of 30.
        Insets insets = Insets.of(0, HEADER_HEIGHT, 0, 0);
        int newWindowWidth = 1000;
        Rect windowRect = new Rect(0, 0, newWindowWidth, WINDOW_HEIGHT);
        // Left block: 10, right block: 20
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, 0, LEFT_BLOCK, HEADER_HEIGHT),
                        new Rect(newWindowWidth - RIGHT_BLOCK, 0, newWindowWidth, HEADER_HEIGHT));
        Rect widestUnoccludedRect =
                new Rect(LEFT_BLOCK, 0, newWindowWidth - RIGHT_BLOCK, HEADER_HEIGHT);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, windowRect);
        notifyInsetsRectObserver();

        verifyDesktopWindowingEnabled();

        var expectedState = new AppHeaderState(windowRect, widestUnoccludedRect, true);
        assertEquals(
                "AppHeaderState is different.",
                expectedState,
                mAppHeaderCoordinator.getAppHeaderState());
        verify(mObserver).onAppHeaderStateChanged(eq(expectedState));
    }

    @Test
    public void initializeWithDesktopWindowingThenExit() {
        setupWithLeftAndRightBoundingRect();
        initAppHeaderCoordinator();
        verifyDesktopWindowingEnabled();

        var expectedState = new AppHeaderState(WINDOW_RECT, WIDEST_UNOCCLUDED_RECT, true);
        assertEquals(
                "AppHeaderState is different.",
                expectedState,
                mAppHeaderCoordinator.getAppHeaderState());

        setupWithNoCaptionInsets();
        notifyInsetsRectObserver();
        verifyDesktopWindowingDisabled(
                /* error= */ "DesktopWindowing should exit when no insets is supplied.");
        verify(mBrowserControlsVisDelegate).releasePersistentShowingToken(anyInt());

        expectedState = new AppHeaderState(WINDOW_RECT, new Rect(), false);
        assertEquals(
                "AppHeaderState is different.",
                expectedState,
                mAppHeaderCoordinator.getAppHeaderState());
        verify(mObserver).onAppHeaderStateChanged(any());
    }

    @Test
    public void testDestroy() {
        mAppHeaderCoordinator.destroy();

        verify(mInsetsRectProvider).destroy();
    }

    @Test
    public void activityLostFocusInDesktopWindow() {
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Assume that the current activity lost focus.
        mAppHeaderCoordinator.onTopResumedActivityChanged(false);

        assertTrue(
                "Window focus state is not correctly set.",
                mAppHeaderCoordinator.isInUnfocusedDesktopWindow());
    }

    @Test
    public void startupInUnfocusedWindow() {
        // Set initial saved instance state value.
        mSavedInstanceStateBundle.putBoolean(INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW, true);
        initAppHeaderCoordinator();

        assertTrue(
                "Window focus state is not correctly set.",
                mAppHeaderCoordinator.isInUnfocusedDesktopWindow());
    }

    @Test
    public void saveInstanceStateForUnfocusedWindow() {
        mSavedInstanceStateBundle.putBoolean(INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW, false);
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Verify initial value.
        assertFalse(
                "Window focus state is not correctly set.",
                mAppHeaderCoordinator.isInUnfocusedDesktopWindow());

        // Assume that the current activity lost focus.
        mAppHeaderCoordinator.onTopResumedActivityChanged(false);
        // Assume that an activity pause triggers saving the instance state.
        mAppHeaderCoordinator.onSaveInstanceState(mSavedInstanceStateBundle);

        assertTrue(mSavedInstanceStateBundle.getBoolean(INSTANCE_STATE_KEY_IS_APP_IN_UNFOCUSED_DW));
    }

    @Test
    public void updateForegroundColor() {
        var insetController = mSpyRootView.getWindowInsetsController();

        mAppHeaderCoordinator.updateForegroundColor(Color.BLACK);
        assertEquals(
                "Background is dark. Expecting APPEARANCE_LIGHT_CAPTION_BARS not set.",
                0,
                insetController.getSystemBarsAppearance() & APPEARANCE_LIGHT_CAPTION_BARS);

        mAppHeaderCoordinator.updateForegroundColor(Color.WHITE);
        assertEquals(
                "Background is light. Expecting APPEARANCE_LIGHT_CAPTION_BARS set.",
                APPEARANCE_LIGHT_CAPTION_BARS,
                insetController.getSystemBarsAppearance() & APPEARANCE_LIGHT_CAPTION_BARS);
    }

    @Test
    public void noImeOrNavBarInsets() {
        // Simulate switching to desktop windowing mode, without any bottom insets.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();
        verify(mSpyRootView, never()).setPadding(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void overlappingKeyboard_SwitchToAndFromDesktopWindowingMode() {
        verifyDesktopWindowingDisabled(
                /* error= */ "DesktopWindowing should exit when no insets is supplied.");

        // Simulate overlapping keyboard.
        var insets = applyWindowInsets(KEYBOARD_INSET, UNSPECIFIED_INSET);
        assertNotEquals(
                "Ime insets should not be consumed when root view is not adjusted.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.ime()));
        assertEquals("Root view bottom should not be padded.", 0, mSpyRootView.getPaddingBottom());

        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();
        insets = applyWindowInsets(KEYBOARD_INSET, UNSPECIFIED_INSET);
        assertEquals(
                "Ime insets should be consumed when root view is bottom-padded.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.ime()));
        verifyDesktopWindowingEnabled();
        assertEquals(
                "Root view bottom padding should be updated.",
                KEYBOARD_INSET,
                mSpyRootView.getPaddingBottom());

        // Simulate switching out of desktop windowing mode.
        setupWithNoCaptionInsets();
        notifyInsetsRectObserver();
        insets = applyWindowInsets(KEYBOARD_INSET, UNSPECIFIED_INSET);
        assertNotEquals(
                "Ime insets should not be consumed when root view is not adjusted.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.ime()));
        assertEquals(
                "Root view bottom padding should be reset.", 0, mSpyRootView.getPaddingBottom());
    }

    @Test
    public void overlappingKeyboard_MoveDesktopWindow() {
        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Simulate overlapping keyboard.
        var insets = applyWindowInsets(KEYBOARD_INSET, UNSPECIFIED_INSET);
        assertEquals(
                "Ime insets should be consumed when root view is adjusted.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.ime()));
        assertEquals(
                "Root view bottom padding should be updated.",
                KEYBOARD_INSET,
                mSpyRootView.getPaddingBottom());

        // Simulate moving a desktop window that causes the keyboard inset to be updated.
        insets = applyWindowInsets(KEYBOARD_INSET + 100, UNSPECIFIED_INSET);
        assertEquals(
                "Ime insets should be consumed when root view is adjusted.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.ime()));
        assertEquals(
                "Root view bottom padding should be updated.",
                KEYBOARD_INSET + 100,
                mSpyRootView.getPaddingBottom());
    }

    @Test
    public void overlappingNavBar_SwitchToAndFromDesktopWindowingMode() {
        verifyDesktopWindowingDisabled(
                /* error= */ "Desktop windowing mode should be disabled initially.");

        // Simulate overlapping nav bar bottom inset.
        var insets = applyWindowInsets(UNSPECIFIED_INSET, NAV_BAR_INSET);
        assertNotEquals(
                "Nav bar insets should not be consumed when root view is not adjusted.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.navigationBars()));
        assertEquals("Root view bottom should not be padded.", 0, mSpyRootView.getPaddingBottom());

        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();
        insets = applyWindowInsets(UNSPECIFIED_INSET, NAV_BAR_INSET);
        assertEquals(
                "Nav bar insets should be consumed when root view is bottom-padded.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.ime()));
        verifyDesktopWindowingEnabled();
        assertEquals(
                "Root view bottom padding should be updated.",
                NAV_BAR_INSET,
                mSpyRootView.getPaddingBottom());

        // Simulate switching out of desktop windowing mode.
        setupWithNoCaptionInsets();
        notifyInsetsRectObserver();
        insets = applyWindowInsets(UNSPECIFIED_INSET, NAV_BAR_INSET);
        assertNotEquals(
                "Nav bar insets should not be consumed when root view is not adjusted.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.navigationBars()));
        assertEquals(
                "Root view bottom padding should be reset.", 0, mSpyRootView.getPaddingBottom());
    }

    @Test
    public void overlappingNavBar_MoveDesktopWindow() {
        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Simulate overlapping nav bar bottom inset.
        var insets = applyWindowInsets(UNSPECIFIED_INSET, NAV_BAR_INSET);
        assertEquals(
                "Nav bar insets should be consumed when root view is adjusted.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.navigationBars()));

        // Simulate moving a desktop window that causes the nav bar inset to be updated.
        insets = applyWindowInsets(UNSPECIFIED_INSET, NAV_BAR_INSET - 10);
        assertEquals(
                "Nav bar insets should be consumed when root view is adjusted.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.navigationBars()));
        assertEquals(
                "Root view bottom padding should be updated.",
                NAV_BAR_INSET - 10,
                mSpyRootView.getPaddingBottom());
    }

    @Test
    public void overlappingKeyboardAndNavBar() {
        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Simulate overlapping keyboard and nav bar bottom insets.
        applyWindowInsets(KEYBOARD_INSET, NAV_BAR_INSET);
        assertEquals(
                "Root view bottom padding should be updated.",
                KEYBOARD_INSET,
                mSpyRootView.getPaddingBottom());
    }

    @Test
    public void windowingModeHistogram_EnterFullScreen() {
        // Simulate starting in desktop windowing mode for an initial state.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Simulate switching to fullscreen mode.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.MultiWindowMode.Configuration",
                                WindowingMode.FULLSCREEN,
                                1)
                        .build();

        doReturn(false).when(mSpyActivity).isInMultiWindowMode();
        setupWithNoCaptionInsets();
        mLastSeenRawWindowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, 10))
                        .build();
        notifyInsetsRectObserver();

        // Histogram should be emitted as expected.
        watcher.assertExpected();
    }

    @Test
    public void windowingModeHistogram_EnterSplitScreen() {
        // Simulate starting in desktop windowing mode for an initial state.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Simulate switching to split screen mode.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.MultiWindowMode.Configuration",
                                WindowingMode.MULTI_WINDOW,
                                1)
                        .build();

        doReturn(true).when(mSpyActivity).isInMultiWindowMode();
        doReturn(false).when(mSpyActivity).isInPictureInPictureMode();
        setupWithNoCaptionInsets();
        mLastSeenRawWindowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, 10))
                        .build();
        notifyInsetsRectObserver();

        // Histogram should be emitted as expected.
        watcher.assertExpected();
    }

    @Test
    public void windowingModeHistogram_EnterPipMode() {
        // Simulate starting in desktop windowing mode for an initial state.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Simulate switching to picture-in-picture mode.
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.MultiWindowMode.Configuration",
                                WindowingMode.PICTURE_IN_PICTURE,
                                1)
                        .build();

        doReturn(true).when(mSpyActivity).isInMultiWindowMode();
        doReturn(true).when(mSpyActivity).isInPictureInPictureMode();
        setupWithNoCaptionInsets();
        mLastSeenRawWindowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, 10))
                        .build();
        notifyInsetsRectObserver();

        // Histogram should be emitted as expected.
        watcher.assertExpected();
    }

    @Test
    public void windowingModeHistogramNotRecordedWhenInsetsAbsent() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.MultiWindowMode.Configuration")
                        .build();
        // Override the last seen raw insets and trigger an insets rect update.
        mLastSeenRawWindowInsets = new WindowInsetsCompat.Builder().build();
        setupWithNoCaptionInsets();
        notifyInsetsRectObserver();

        // Histogram should not be emitted.
        watcher.assertExpected();
    }

    @Test
    public void windowingModeHistogramRecordedOnce() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.MultiWindowMode.Configuration", WindowingMode.DESKTOP_WINDOW);
        setupWithLeftAndRightBoundingRect();
        // Simulate multiple rect updates that will be triggered when windowing mode changes.
        notifyInsetsRectObserver();
        notifyInsetsRectObserver();

        // Histogram should be emitted just once.
        watcher.assertExpected();
    }

    private void initAppHeaderCoordinator() {
        mAppHeaderCoordinator =
                new AppHeaderCoordinator(
                        mSpyActivity,
                        mSpyRootView,
                        mBrowserControlsVisDelegate,
                        mInsetObserver,
                        mActivityLifecycleDispatcher,
                        mSavedInstanceStateBundle,
                        mEdgeToEdgeStateProvider);
        mAppHeaderCoordinator.addObserver(mObserver);
    }

    private void setupWithNoCaptionInsets() {
        setupInsetsRectProvider(Insets.NONE, List.of(), new Rect(), WINDOW_RECT);
    }

    private void setupWithLeftAndRightBoundingRect() {
        // Top insets with height of 30.
        Insets insets = Insets.of(0, HEADER_HEIGHT, 0, 0);
        // Left block: 10, right block: 20
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, 0, LEFT_BLOCK, HEADER_HEIGHT),
                        new Rect(WINDOW_WIDTH - RIGHT_BLOCK, 0, WINDOW_WIDTH, HEADER_HEIGHT));
        Rect widestUnoccludedRect =
                new Rect(LEFT_BLOCK, 0, WINDOW_WIDTH - RIGHT_BLOCK, HEADER_HEIGHT);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, WINDOW_RECT);
    }

    private void setupInsetsRectProvider(
            Insets insets, List<Rect> blockedRects, Rect widestUnOccludedRect, Rect windowRect) {
        mLastSeenRawWindowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.captionBar(), insets)
                        .build();

        doReturn(windowRect).when(mInsetsRectProvider).getWindowRect();
        doReturn(widestUnOccludedRect).when(mInsetsRectProvider).getWidestUnoccludedRect();
        doReturn(insets).when(mInsetsRectProvider).getCachedInset();
        doReturn(blockedRects).when(mInsetsRectProvider).getBoundingRects();
    }

    private void notifyInsetsRectObserver() {
        verify(mInsetsRectProvider, atLeastOnce()).addObserver(mInsetRectObserverCaptor.capture());
        mInsetRectObserverCaptor
                .getValue()
                .onBoundingRectsUpdated(mInsetsRectProvider.getWidestUnoccludedRect());
    }

    private void verifyDesktopWindowingEnabled() {
        assertTrue(
                "Desktop windowing not enabled.",
                mAppHeaderCoordinator.getAppHeaderState().isInDesktopWindow());
        verify(mBrowserControlsVisDelegate, atLeastOnce())
                .showControlsPersistentAndClearOldToken(anyInt());
        assertTrue("Edge to edge should be active.", mEdgeToEdgeStateProvider.get());
    }

    private void verifyDesktopWindowingDisabled(String error) {
        assertFalse(
                error,
                mAppHeaderCoordinator.getAppHeaderState() != null
                        && mAppHeaderCoordinator.getAppHeaderState().isInDesktopWindow());
        assertFalse("Edge to edge should not be active.", mEdgeToEdgeStateProvider.get());
    }

    private WindowInsetsCompat applyWindowInsets(int keyboardInset, int navBarInset) {
        var windowInsetsBuilder = new WindowInsetsCompat.Builder();
        if (keyboardInset != UNSPECIFIED_INSET) {
            windowInsetsBuilder.setInsets(
                    WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, keyboardInset));
        }
        if (navBarInset != UNSPECIFIED_INSET) {
            windowInsetsBuilder.setInsets(
                    WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, navBarInset));
        }
        return mAppHeaderCoordinator.onApplyWindowInsets(mSpyRootView, windowInsetsBuilder.build());
    }

    private void updateFeatureParams(boolean enableOnExternalDisplay, String oemDenylist) {
        FeatureOverrides.Builder overrides =
                FeatureOverrides.newBuilder()
                        .enable(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
                        .param(
                                "enable_on_external_display",
                                enableOnExternalDisplay ? "true" : "false")
                        .param("external_display_oem_denylist", oemDenylist);
        overrides.apply();
    }
}

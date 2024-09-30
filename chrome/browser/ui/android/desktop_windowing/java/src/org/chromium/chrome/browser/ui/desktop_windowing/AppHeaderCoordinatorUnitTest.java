// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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
import android.graphics.Color;
import android.graphics.Rect;
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
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils.DesktopWindowHeuristicResult;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetObserver.WindowInsetObserver;
import org.chromium.ui.InsetsRectProvider;
import org.chromium.ui.base.TestActivity;

import java.util.List;

/** Unit test for {@link AppHeaderCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30)
@LooperMode(Mode.PAUSED)
public class AppHeaderCoordinatorUnitTest {
    private static final int WINDOW_WIDTH = 600;
    private static final int WINDOW_HEIGHT = 800;
    private static final Rect WINDOW_RECT = new Rect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    private static final int LEFT_BLOCK = 10;
    private static final int RIGHT_BLOCK = 20;
    private static final int HEADER_HEIGHT = 30;
    private static final Rect WIDEST_UNOCCLUDED_RECT =
            new Rect(LEFT_BLOCK, 0, WINDOW_WIDTH - RIGHT_BLOCK, HEADER_HEIGHT);
    private static final int KEYBOARD_INSET = 672;
    private static final int SYSTEM_BAR_BOTTOM_INSET = 64;

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisDelegate;
    @Mock private InsetObserver mInsetObserver;
    @Mock private InsetsRectProvider mInsetsRectProvider;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private DesktopWindowStateProvider.AppHeaderObserver mObserver;
    @Captor private ArgumentCaptor<InsetsRectProvider.Observer> mInsetRectObserverCaptor;

    private AppHeaderCoordinator mAppHeaderCoordinator;
    private Activity mSpyActivity;
    private View mSpyRootView;
    private WindowInsetsCompat mLastSeenRawWindowInsets = new WindowInsetsCompat(null);
    private Bundle mSavedInstanceStateBundle;
    private WindowInsetObserver mWindowInsetObserver;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mSpyActivity = spy(activity));
        doReturn(true).when(mSpyActivity).isInMultiWindowMode();
        mSpyRootView = spy(mSpyActivity.getWindow().getDecorView());
        AppHeaderCoordinator.setInsetsRectProviderForTesting(mInsetsRectProvider);
        doAnswer(inv -> mLastSeenRawWindowInsets).when(mInsetObserver).getLastRawWindowInsets();
        setupWithNoInsets();
        mSavedInstanceStateBundle = new Bundle();
        initAppHeaderCoordinator();
    }

    @Test
    public void notEnabledWithNoTopInsets() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult",
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

        assertFalse(
                "Desktop Windowing not enabled for bottom insets.",
                mAppHeaderCoordinator.isInDesktopWindow());
        watcher.assertExpected();
    }

    @Test
    public void notEnabledWithBoundingRectsWithPartialHeight() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult",
                        DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT);
        // Bottom insets with height = 30
        Insets insets = Insets.of(0, 30, 0, 0);
        // Left block: 10, right block: 20. Bounding rect is not at the full height.
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, 0, LEFT_BLOCK, HEADER_HEIGHT - 10),
                        new Rect(WINDOW_WIDTH - RIGHT_BLOCK, 0, WINDOW_WIDTH, HEADER_HEIGHT - 10));
        Rect widestUnoccludedRect = new Rect(0, 20, WINDOW_WIDTH, HEADER_HEIGHT - 10);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, WINDOW_RECT);
        notifyInsetsRectObserver();

        assertFalse(
                "Desktop Windowing enabled for widestUnOccludedRect with less height "
                        + " than the insets.",
                mAppHeaderCoordinator.isInDesktopWindow());
        watcher.assertExpected();
    }

    @Test
    public void notEnabledWithLessThanTwoBoundingRects() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult",
                        DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECTS_UNEXPECTED_NUMBER);
        // Top insets with height of 30.
        Insets insets = Insets.of(0, 30, 0, 0);
        // Left block: 10
        List<Rect> blockedRects = List.of(new Rect(0, 0, LEFT_BLOCK, 30));
        Rect widestUnoccludedRect = new Rect(LEFT_BLOCK, 0, WINDOW_WIDTH, 30);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, WINDOW_RECT);
        notifyInsetsRectObserver();

        assertFalse(
                "Desktop Windowing enabled with only one bounding rect.",
                mAppHeaderCoordinator.isInDesktopWindow());
        watcher.assertExpected();
    }

    @Test
    public void notEnabledWithMoreThanTwoBoundingRects() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult",
                        DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECTS_UNEXPECTED_NUMBER);
        // Top insets with height of 30.
        Insets insets = Insets.of(0, 30, 0, 0);
        // Left block: 10, two right block: 5, 15
        List<Rect> blockedRects =
                List.of(
                        new Rect(0, 0, LEFT_BLOCK, 30),
                        new Rect(WINDOW_WIDTH - RIGHT_BLOCK, 0, WINDOW_WIDTH - 15, 30),
                        new Rect(WINDOW_WIDTH - 15, 0, WINDOW_WIDTH, 30));
        Rect widestUnoccludedRect = new Rect(LEFT_BLOCK, 0, WINDOW_WIDTH - RIGHT_BLOCK, 30);
        setupInsetsRectProvider(insets, blockedRects, widestUnoccludedRect, WINDOW_RECT);
        notifyInsetsRectObserver();

        assertFalse(
                "Desktop Windowing enabled with more than two bounding rects.",
                mAppHeaderCoordinator.isInDesktopWindow());
        watcher.assertExpected();
    }

    @Test
    public void notEnabledWhenNotInMultiWindowMode() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult",
                        DesktopWindowHeuristicResult.NOT_IN_MULTIWINDOW_MODE);
        doReturn(false).when(mSpyActivity).isInMultiWindowMode();
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        assertFalse(
                "Desktop Windowing does not enable when not in multi window mode.",
                mAppHeaderCoordinator.isInDesktopWindow());
        watcher.assertExpected();
    }

    @Test
    public void notEnabledWhenNavBarBottomInsetsSeen() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult",
                        DesktopWindowHeuristicResult.NAV_BAR_BOTTOM_INSETS_PRESENT);
        setupWithLeftAndRightBoundingRect();
        // Override the last seen raw insets so there's a bottom nav bar insets.
        mLastSeenRawWindowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, 10))
                        .build();
        notifyInsetsRectObserver();

        assertFalse(
                "Desktop Windowing does not enable when there are bottom insets.",
                mAppHeaderCoordinator.isInDesktopWindow());
        watcher.assertExpected();
    }

    @Test
    public void enableDesktopWindowing() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DesktopWindowHeuristicResult",
                        DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW);
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
                        .expectAnyRecordTimes("Android.DesktopWindowHeuristicResult", 1)
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

        setupWithNoInsets();
        notifyInsetsRectObserver();
        assertFalse(
                "DesktopWindowing should exit when no insets is supplied.",
                mAppHeaderCoordinator.isInDesktopWindow());
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
                insetController.getSystemBarsAppearance() & (1 << 8));

        mAppHeaderCoordinator.updateForegroundColor(Color.WHITE);
        assertEquals(
                "Background is light. Expecting APPEARANCE_LIGHT_CAPTION_BARS set.",
                (1 << 8),
                insetController.getSystemBarsAppearance() & (1 << 8));
    }

    @Test
    public void noBottomSystemOrImeInsets() {
        // Simulate switching to desktop windowing mode, without any bottom insets.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();
        verify(mSpyRootView, never()).setPadding(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void overlappingKeyboard_SwitchToAndFromDesktopWindowingMode() {
        assertFalse(
                "Desktop windowing mode should be disabled initially.",
                mAppHeaderCoordinator.isInDesktopWindow());

        // Simulate overlapping keyboard.
        mWindowInsetObserver.onKeyboardInsetChanged(KEYBOARD_INSET);
        assertEquals("Root view bottom should not be padded.", 0, mSpyRootView.getPaddingBottom());

        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();
        verifyDesktopWindowingEnabled();
        assertEquals(
                "Root view bottom padding should be updated.",
                KEYBOARD_INSET,
                mSpyRootView.getPaddingBottom());

        // Simulate switching out of desktop windowing mode.
        setupWithNoInsets();
        notifyInsetsRectObserver();
        assertEquals(
                "Root view bottom padding should be reset.", 0, mSpyRootView.getPaddingBottom());
    }

    @Test
    public void overlappingKeyboard_MoveDesktopWindow() {
        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Simulate overlapping keyboard.
        mWindowInsetObserver.onKeyboardInsetChanged(KEYBOARD_INSET);
        assertEquals(
                "Root view bottom padding should be updated.",
                KEYBOARD_INSET,
                mSpyRootView.getPaddingBottom());

        // Simulate moving a desktop window that causes the keyboard inset to be updated.
        mWindowInsetObserver.onKeyboardInsetChanged(KEYBOARD_INSET + 100);
        assertEquals(
                "Root view bottom padding should be updated.",
                KEYBOARD_INSET + 100,
                mSpyRootView.getPaddingBottom());
    }

    @Test
    public void overlappingBottomSystemBar_SwitchToAndFromDesktopWindowingMode() {
        assertFalse(
                "Desktop windowing mode should be disabled initially.",
                mAppHeaderCoordinator.isInDesktopWindow());

        // Simulate overlapping system bar bottom inset.
        mWindowInsetObserver.onInsetChanged(0, 0, 0, SYSTEM_BAR_BOTTOM_INSET);
        assertEquals("Root view bottom should not be padded.", 0, mSpyRootView.getPaddingBottom());

        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();
        verifyDesktopWindowingEnabled();
        assertEquals(
                "Root view bottom padding should be updated.",
                SYSTEM_BAR_BOTTOM_INSET,
                mSpyRootView.getPaddingBottom());

        // Simulate switching out of desktop windowing mode.
        setupWithNoInsets();
        notifyInsetsRectObserver();
        assertEquals(
                "Root view bottom padding should be reset.", 0, mSpyRootView.getPaddingBottom());
    }

    @Test
    public void overlappingBottomSystemBar_MoveDesktopWindow() {
        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Simulate overlapping system bar bottom inset.
        mWindowInsetObserver.onInsetChanged(0, 0, 0, SYSTEM_BAR_BOTTOM_INSET);
        assertEquals(
                "Root view bottom padding should be updated.",
                SYSTEM_BAR_BOTTOM_INSET,
                mSpyRootView.getPaddingBottom());

        // Simulate moving a desktop window that causes the system bar inset to be updated.
        mWindowInsetObserver.onInsetChanged(0, 0, 0, SYSTEM_BAR_BOTTOM_INSET - 10);
        assertEquals(
                "Root view bottom padding should be updated.",
                SYSTEM_BAR_BOTTOM_INSET - 10,
                mSpyRootView.getPaddingBottom());
    }

    @Test
    public void overlappingKeyboardAndBottomSystemBar() {
        // Simulate switching to desktop windowing mode.
        setupWithLeftAndRightBoundingRect();
        notifyInsetsRectObserver();

        // Simulate overlapping keyboard and system bar bottom insets.
        mWindowInsetObserver.onKeyboardInsetChanged(KEYBOARD_INSET);
        mWindowInsetObserver.onInsetChanged(0, 0, 0, SYSTEM_BAR_BOTTOM_INSET);
        assertEquals(
                "Root view bottom padding should be updated.",
                KEYBOARD_INSET,
                mSpyRootView.getPaddingBottom());
    }

    private void initAppHeaderCoordinator() {
        mAppHeaderCoordinator =
                new AppHeaderCoordinator(
                        mSpyActivity,
                        mSpyRootView,
                        mBrowserControlsVisDelegate,
                        mInsetObserver,
                        mActivityLifecycleDispatcher,
                        mSavedInstanceStateBundle);
        mAppHeaderCoordinator.addObserver(mObserver);
        mWindowInsetObserver = mAppHeaderCoordinator.getWindowInsetObserverForTesting();
    }

    private void setupWithNoInsets() {
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
        assertTrue("Desktop windowing not enabled.", mAppHeaderCoordinator.isInDesktopWindow());
        verify(mBrowserControlsVisDelegate, atLeastOnce())
                .showControlsPersistentAndClearOldToken(anyInt());
    }
}

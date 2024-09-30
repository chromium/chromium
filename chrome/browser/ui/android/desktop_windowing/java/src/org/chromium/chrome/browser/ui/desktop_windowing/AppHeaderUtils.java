// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;

/** Utility class for the desktop windowing feature implementation. */
// TODO (crbug/328055199): Rename this to DesktopWindowUtils.
public class AppHeaderUtils {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        DesktopWindowHeuristicResult.UNKNOWN,
        DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW,
        DesktopWindowHeuristicResult.NOT_IN_MULTIWINDOW_MODE,
        DesktopWindowHeuristicResult.NAV_BAR_BOTTOM_INSETS_PRESENT,
        DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECTS_UNEXPECTED_NUMBER,
        DesktopWindowHeuristicResult.CAPTION_BAR_TOP_INSETS_ABSENT,
        DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT,
        DesktopWindowHeuristicResult.NUM_ENTRIES,
    })
    public @interface DesktopWindowHeuristicResult {
        int UNKNOWN = 0;
        int IN_DESKTOP_WINDOW = 1;
        int NOT_IN_MULTIWINDOW_MODE = 2;
        int NAV_BAR_BOTTOM_INSETS_PRESENT = 3;
        int CAPTION_BAR_BOUNDING_RECTS_UNEXPECTED_NUMBER = 4;
        int CAPTION_BAR_TOP_INSETS_ABSENT = 5;
        int CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT = 6;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 7;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        DesktopWindowModeState.UNAVAILABLE,
        DesktopWindowModeState.INACTIVE,
        DesktopWindowModeState.ACTIVE,
    })
    public @interface DesktopWindowModeState {
        int UNAVAILABLE = 0;
        int INACTIVE = 1;
        int ACTIVE = 2;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 3;
    }

    private static Boolean sIsAppInDesktopWindowForTesting;

    /**
     * Determines whether the currently starting activity is focused, based on the {@link
     * ActivityLifecycleDispatcher} instance associated with it. Note that this method is intended
     * to be used during app startup flows and may not return the correct value at other times.
     *
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} instance associated with
     *     the current activity. When {@code null}, it will be assumed that the starting activity is
     *     focused.
     * @return {@code true} if the currently starting activity is focused, {@code false} otherwise.
     */
    public static boolean isActivityFocusedAtStartup(
            @Nullable ActivityLifecycleDispatcher lifecycleDispatcher) {
        return lifecycleDispatcher == null
                || lifecycleDispatcher.getCurrentActivityState()
                        <= ActivityState.RESUMED_WITH_NATIVE;
    }

    /**
     * @param desktopWindowStateProvider The {@link DesktopWindowStateProvider} instance.
     * @return {@code true} if the current activity is in a desktop window, {@code false} otherwise.
     */
    public static boolean isAppInDesktopWindow(
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
        if (sIsAppInDesktopWindowForTesting != null) return sIsAppInDesktopWindowForTesting;
        if (desktopWindowStateProvider == null) return false;
        var appHeaderState = desktopWindowStateProvider.getAppHeaderState();

        return appHeaderState != null && appHeaderState.isInDesktopWindow();
    }

    /**
     * Records the result of the heuristics used to determine whether the app is in a desktop
     * window.
     *
     * @param result The {@link DesktopWindowHeuristicResult} to record.
     */
    public static void recordDesktopWindowHeuristicResult(
            @DesktopWindowHeuristicResult int result) {
        assert result != DesktopWindowHeuristicResult.UNKNOWN;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DesktopWindowHeuristicResult",
                result,
                DesktopWindowHeuristicResult.NUM_ENTRIES);
    }

    /**
     * Records an enumerated histogram using {@link DesktopWindowModeState}.
     *
     * @param desktopWindowStateProvider The {@link DesktopWindowStateProvider} instance.
     * @param histogramName The name of the histogram.
     */
    public static void recordDesktopWindowModeStateEnumHistogram(
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider, String histogramName) {
        @DesktopWindowModeState int state;
        // |desktopWindowStateProvider| will be null on a device that does not support desktop
        // windowing.
        if (desktopWindowStateProvider == null) {
            state = DesktopWindowModeState.UNAVAILABLE;
        } else {
            state =
                    isAppInDesktopWindow(desktopWindowStateProvider)
                            ? DesktopWindowModeState.ACTIVE
                            : DesktopWindowModeState.INACTIVE;
        }
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, state, DesktopWindowModeState.NUM_ENTRIES);
    }

    /**
     * Sets the desktop windowing mode for tests.
     *
     * @param isAppInDesktopWindow Whether desktop windowing mode is activated.
     */
    public static void setAppInDesktopWindowForTesting(boolean isAppInDesktopWindow) {
        sIsAppInDesktopWindowForTesting = isAppInDesktopWindow;
        ResettersForTesting.register(() -> sIsAppInDesktopWindowForTesting = null);
    }
}

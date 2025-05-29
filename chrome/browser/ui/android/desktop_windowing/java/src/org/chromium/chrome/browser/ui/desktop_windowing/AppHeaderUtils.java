// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import android.app.Activity;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;

import java.util.Collections;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/** Utility class for the desktop windowing feature implementation. */
// TODO (crbug/328055199): Rename this to DesktopWindowUtils.
@NullMarked
public class AppHeaderUtils {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        DesktopWindowHeuristicResult.UNKNOWN,
        DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW,
        DesktopWindowHeuristicResult.CAPTION_BAR_TOP_INSETS_ABSENT,
        DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT,
        DesktopWindowHeuristicResult.WIDEST_UNOCCLUDED_RECT_EMPTY,
        DesktopWindowHeuristicResult.DISALLOWED_ON_EXTERNAL_DISPLAY,
        DesktopWindowHeuristicResult.NUM_ENTRIES,
    })
    public @interface DesktopWindowHeuristicResult {
        int UNKNOWN = 0;
        int IN_DESKTOP_WINDOW = 1;
        int CAPTION_BAR_TOP_INSETS_ABSENT = 2;
        int CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT = 3;
        int WIDEST_UNOCCLUDED_RECT_EMPTY = 4;
        int DISALLOWED_ON_EXTERNAL_DISPLAY = 5;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 6;
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

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        WindowingMode.UNKNOWN,
        WindowingMode.FULLSCREEN,
        WindowingMode.PICTURE_IN_PICTURE,
        WindowingMode.DESKTOP_WINDOW,
        WindowingMode.MULTI_WINDOW,
    })
    public @interface WindowingMode {
        int UNKNOWN = 0;
        int FULLSCREEN = 1;
        int PICTURE_IN_PICTURE = 2;
        int DESKTOP_WINDOW = 3;
        int MULTI_WINDOW = 4;

        // Be sure to also update enums.xml when updating these values.
        int NUM_ENTRIES = 5;
    }

    private static @Nullable Boolean sHeaderCustomizationDisallowedOnExternalDisplayForOem;
    private static @Nullable Boolean sIsAppInDesktopWindowForTesting;

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
     * @param desktopWindowStateManager The {@link DesktopWindowStateManager} instance.
     * @return {@code true} if the current activity is in a desktop window, {@code false} otherwise.
     */
    public static boolean isAppInDesktopWindow(
            @Nullable DesktopWindowStateManager desktopWindowStateManager) {
        if (sIsAppInDesktopWindowForTesting != null) return sIsAppInDesktopWindowForTesting;
        if (desktopWindowStateManager == null) return false;
        var appHeaderState = desktopWindowStateManager.getAppHeaderState();

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
                "Android.DesktopWindowHeuristicResult3",
                result,
                DesktopWindowHeuristicResult.NUM_ENTRIES);
    }

    /**
     * Records an enumerated histogram using {@link DesktopWindowModeState}.
     *
     * @param desktopWindowStateManager The {@link DesktopWindowStateManager} instance.
     * @param histogramName The name of the histogram.
     */
    public static void recordDesktopWindowModeStateEnumHistogram(
            @Nullable DesktopWindowStateManager desktopWindowStateManager, String histogramName) {
        @DesktopWindowModeState int state;
        // |desktopWindowStateManager| will be null on a device that does not support desktop
        // windowing.
        if (desktopWindowStateManager == null) {
            state = DesktopWindowModeState.UNAVAILABLE;
        } else {
            state =
                    isAppInDesktopWindow(desktopWindowStateManager)
                            ? DesktopWindowModeState.ACTIVE
                            : DesktopWindowModeState.INACTIVE;
        }
        RecordHistogram.recordEnumeratedHistogram(
                histogramName, state, DesktopWindowModeState.NUM_ENTRIES);
    }

    /**
     * Returns the {@link WindowingMode} in which the app is running.
     *
     * @param activity The {@link Activity} that is running in the window.
     * @param isInDesktopWindow Whether the app is running in a desktop window.
     * @param currentMode The current {@link WindowingMode}.
     */
    public static int getWindowingMode(
            Activity activity, boolean isInDesktopWindow, int currentMode) {
        @WindowingMode int newMode;
        if (isInDesktopWindow) {
            newMode = WindowingMode.DESKTOP_WINDOW;
        } else if (activity.isInPictureInPictureMode()) {
            newMode = WindowingMode.PICTURE_IN_PICTURE;
        } else {
            newMode =
                    activity.isInMultiWindowMode()
                            ? WindowingMode.MULTI_WINDOW
                            : WindowingMode.FULLSCREEN;
        }
        if (newMode != currentMode) {
            // Record histogram only when the windowing mode changes.
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.MultiWindowMode.Configuration", newMode, WindowingMode.NUM_ENTRIES);
        }
        return newMode;
    }

    /**
     * @return {@code true} if app header customization should be allowed on an external display,
     *     {@code false} otherwise.
     */
    public static boolean shouldAllowHeaderCustomizationOnNonDefaultDisplay() {
        // Determine if app header customization will be ignored on the external display on specific
        // OEMs.
        if (sHeaderCustomizationDisallowedOnExternalDisplayForOem == null) {
            Set<String> denylist = new HashSet<>();
            String denylistStr =
                    ChromeFeatureList.sTabStripLayoutOptimizationOnExternalDisplayOemDenylist
                            .getValue();
            if (!TextUtils.isEmpty(denylistStr)) {
                Collections.addAll(denylist, denylistStr.split(","));
            }
            sHeaderCustomizationDisallowedOnExternalDisplayForOem =
                    !denylist.isEmpty()
                            && denylist.contains(Build.MANUFACTURER.toLowerCase(Locale.US));
        }
        if (sHeaderCustomizationDisallowedOnExternalDisplayForOem) {
            return false;
        }

        return ChromeFeatureList.sTabStripLayoutOptimizationOnExternalDisplay.getValue();
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

    /** Resets |sHeaderCustomizationDisallowedOnExternalDisplayForOem| in tests. */
    public static void resetHeaderCustomizationDisallowedOnExternalDisplayForOemForTesting() {
        sHeaderCustomizationDisallowedOnExternalDisplayForOem = null;
    }
}

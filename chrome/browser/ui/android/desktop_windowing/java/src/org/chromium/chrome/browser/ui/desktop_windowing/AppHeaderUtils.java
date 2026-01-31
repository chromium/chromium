// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static android.os.Build.VERSION.SDK_INT;

import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION_CODES;

import androidx.annotation.IntDef;
import androidx.core.graphics.Insets;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.insets.InsetsRectProvider;

import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

/** Utility class for the desktop windowing feature implementation. */
// TODO (crbug/328055199): Rename this to DesktopWindowUtils.
@NullMarked
public class AppHeaderUtils {
    // External OEMs for which app header customization will be disabled on external displays.
    private static final Set<String> EXTERNAL_DISPLAY_OEM_DENYLIST = new HashSet<>();

    static {
        // Samsung added a bugfix in Android 16 that is required for Chrome app header customization
        // to work correctly on external displays. Prior to this version, we disallow the feature
        // for Chrome running on external displays connected to all Samsung devices. See
        // crbug.com/455925279 for details.
        if (SDK_INT < VERSION_CODES.BAKLAVA) {
            EXTERNAL_DISPLAY_OEM_DENYLIST.add("samsung");
        }
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        DesktopWindowHeuristicResult.UNKNOWN,
        DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW,
        DesktopWindowHeuristicResult.CAPTION_BAR_TOP_INSETS_ABSENT,
        DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT,
        DesktopWindowHeuristicResult.WIDEST_UNOCCLUDED_RECT_EMPTY,
        DesktopWindowHeuristicResult.DISALLOWED_ON_EXTERNAL_DISPLAY,
        DesktopWindowHeuristicResult.COMPLEX_UNOCCLUDED_REGION,
        DesktopWindowHeuristicResult.NUM_ENTRIES,
    })
    public @interface DesktopWindowHeuristicResult {
        int UNKNOWN = 0;
        int IN_DESKTOP_WINDOW = 1;
        int CAPTION_BAR_TOP_INSETS_ABSENT = 2;
        int CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT = 3;
        int WIDEST_UNOCCLUDED_RECT_EMPTY = 4;
        int DISALLOWED_ON_EXTERNAL_DISPLAY = 5;
        int COMPLEX_UNOCCLUDED_REGION = 6;

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
                "Android.DesktopWindowHeuristicResult4",
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
     * Check if the desktop windowing mode is enabled by checking all the criteria:
     *
     * <ol type=1>
     *   <li>Caption bar has insets.top > 0;
     *   <li>Widest unoccluded rect in caption bar has space available to draw the tab strip;
     *   <li>Widest unoccluded rect in captionBar insets is connected to the bottom;
     *   <li>Header customization is not disallowed;
     *   <li>Unoccluded space in the caption bar is complex;
     * </ol>
     */
    static @DesktopWindowHeuristicResult int checkIsInDesktopWindow(
            InsetsRectProvider insetsRectProvider, Context context) {
        @DesktopWindowHeuristicResult int newResult;

        boolean isOnExternalDisplay = !DisplayUtil.isContextInDefaultDisplay(context);

        Insets captionBarInset = insetsRectProvider.getCachedInset();
        boolean allowHeaderCustomization =
                AppHeaderUtils.shouldAllowHeaderCustomizationOnNonDefaultDisplay()
                        || !isOnExternalDisplay;

        if (insetsRectProvider.getWidestUnoccludedRect().isEmpty()) {
            newResult = DesktopWindowHeuristicResult.WIDEST_UNOCCLUDED_RECT_EMPTY;
        } else if (captionBarInset.top == 0) {
            newResult = DesktopWindowHeuristicResult.CAPTION_BAR_TOP_INSETS_ABSENT;
        } else if (insetsRectProvider.getWidestUnoccludedRect().bottom != captionBarInset.top) {
            newResult = DesktopWindowHeuristicResult.CAPTION_BAR_BOUNDING_RECT_INVALID_HEIGHT;
        } else if (!allowHeaderCustomization) {
            newResult = DesktopWindowHeuristicResult.DISALLOWED_ON_EXTERNAL_DISPLAY;
        } else if (insetsRectProvider.isUnoccludedRegionComplex()) {
            newResult = DesktopWindowHeuristicResult.COMPLEX_UNOCCLUDED_REGION;
        } else {
            newResult = DesktopWindowHeuristicResult.IN_DESKTOP_WINDOW;
        }
        return newResult;
    }

    /**
     * @return {@code true} if app header customization should be allowed on an external display,
     *     {@code false} otherwise.
     */
    public static boolean shouldAllowHeaderCustomizationOnNonDefaultDisplay() {
        // Determine if app header customization will be ignored on the external display on specific
        // OEMs.
        if (sHeaderCustomizationDisallowedOnExternalDisplayForOem == null) {
            sHeaderCustomizationDisallowedOnExternalDisplayForOem =
                    !EXTERNAL_DISPLAY_OEM_DENYLIST.isEmpty()
                            && EXTERNAL_DISPLAY_OEM_DENYLIST.contains(
                                    Build.MANUFACTURER.toLowerCase(Locale.US));
        }
        return !sHeaderCustomizationDisallowedOnExternalDisplayForOem;
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

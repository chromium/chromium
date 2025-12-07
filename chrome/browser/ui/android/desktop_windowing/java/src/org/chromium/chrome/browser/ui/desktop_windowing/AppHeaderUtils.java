// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.desktop_windowing;

import static android.os.Build.VERSION.SDK_INT;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.core.graphics.Insets;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher.ActivityState;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
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
    private static final long CYCLE_LENGTH_MS = DateUtils.DAY_IN_MILLIS;

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
     * Returns the {@link WindowingMode} in which the app is running.
     *
     * @param activity The {@link Activity} that is running in the window.
     * @param isInDesktopWindow Whether the app is running in a desktop window.
     */
    public static int getWindowingMode(Activity activity, boolean isInDesktopWindow) {
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
        return newMode;
    }

    /**
     * Updates the activity count for a given windowing mode and starts or stops the clock for
     * tracking time spent in that mode.
     *
     * <p>This method uses the activity count to determine if there is at least one active activity
     * in the given mode. When the first activity enters a mode, it starts a timer. When the last
     * activity in a given mode is stopped, it stops the timer and records the duration.
     *
     * @param mode The {@link WindowingMode} to update.
     * @param isStarted {@code true} if an activity is entering this mode, {@code false} if it is
     *     exiting.
     */
    // TODO(crbug.com/454020115): Relocate this method and helpers to a new utils class.
    public static void recordWindowingMode(int mode, boolean isStarted) {
        if (mode == WindowingMode.UNKNOWN) return;
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        String key = ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITY_COUNT.createKey(mode);
        int prevCount = prefs.readInt(key, 0);
        int newCount = isStarted ? prevCount + 1 : prevCount - 1;
        prefs.writeInt(key, newCount);
        if (prevCount == 0 && newCount == 1) {
            startOrStopClockForWindowingMode(mode, /* startClock= */ true);
        } else if (prevCount == 1 && newCount == 0) {
            startOrStopClockForWindowingMode(mode, /* startClock= */ false);
        }
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

    private static void startOrStopClockForWindowingMode(int mode, boolean startClock) {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        long currentTime = TimeUtils.currentTimeMillis();
        if (startClock) {
            String key = ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(mode);
            prefs.writeLong(key, currentTime);
        } else {
            recordTimeSpentInWindowingMode(mode);
        }
    }

    /**
     * Records the time spent in a given windowing mode.
     *
     * <p>This method uses a cycling mechanism to batch histogram reports. If at least one cycle
     * (defined by {@link #CYCLE_LENGTH_MS}) has passed since the last report, it finalizes the
     * metrics for the previous cycles, records them to a histogram, and carries over any time from
     * the current mode into the next cycle. If still within the same cycle, it simply updates the
     * total duration for the given mode.
     *
     * @param mode The windowing mode to record duration for.
     * @param currentTime The current timestamp.
     */
    private static void recordTimeSpentInWindowingMode(int stoppedMode) {
        long currentTime = TimeUtils.currentTimeMillis();
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        if (stoppedMode == WindowingMode.UNKNOWN) return;

        long cycleStartTime =
                prefs.readLong(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, currentTime);
        // While at least one cycle has elapsed since the current cycle start time, update the
        // running durations for each mode and record the histogram.
        while (cycleStartTime + CYCLE_LENGTH_MS <= currentTime) {

            long cycleEndTime = cycleStartTime + CYCLE_LENGTH_MS;
            for (int modeIndex = 1; modeIndex < WindowingMode.NUM_ENTRIES; modeIndex++) {
                int modeActivityCount =
                        prefs.readInt(
                                ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITY_COUNT.createKey(
                                        modeIndex),
                                0);
                String startTimeKey =
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(modeIndex);
                String durationKey =
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(modeIndex);
                long modeStartTime = prefs.readLong(startTimeKey, currentTime);

                // Update the duration for the mode if it has at least one activity open or if the
                // mode is currently being stopped.
                if (modeActivityCount > 0 || modeIndex == stoppedMode) {
                    // In both cases, we can safely assume that the mode was active until the end of
                    // the current cycle.
                    long durationMs = prefs.readLong(durationKey, 0);
                    durationMs += (cycleEndTime - modeStartTime);
                    prefs.writeLong(durationKey, durationMs);
                    // Update the start time of the mode to the end of the current cycle because at
                    // this point, it is still considered active.
                    prefs.writeLong(startTimeKey, cycleEndTime);
                }

                recordWindowingModeHistogram(modeIndex);
            }

            // Update the cycle start time to the end of the current cycle. This will be used as the
            // start time for the next cycle.
            prefs.writeLong(ChromePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, cycleEndTime);
            cycleStartTime = cycleEndTime;
        }

        // Update the duration for the mode that is being stopped and remove the start time key.
        String startTimeKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(stoppedMode);
        long modeStartTime = prefs.readLong(startTimeKey, currentTime);
        String durationKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(stoppedMode);
        long durationMs = prefs.readLong(durationKey, 0);
        durationMs += currentTime - modeStartTime;
        prefs.writeLong(durationKey, durationMs);
        // Remove the start time key as we are done tracking the duration.
        prefs.removeKey(startTimeKey);
    }

    private static void recordWindowingModeHistogram(int mode) {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        String modeDurationKey = ChromePreferenceKeys.MULTI_WINDOW_MODE_DURATION_MS.createKey(mode);
        long modeDurationMs = prefs.readLong(modeDurationKey, 0);
        String histogramVariant = getWindowingModeHistogramName(mode);
        if (modeDurationMs > 0) {
            assert modeDurationMs <= CYCLE_LENGTH_MS;
            RecordHistogram.recordLongTimesHistogram(
                    "Android.MultiWindowMode." + histogramVariant + ".Duration", modeDurationMs);
        }
        // Remove the duration key for the mode.
        prefs.removeKey(modeDurationKey);
    }

    private static String getWindowingModeHistogramName(int mode) {
        switch (mode) {
            case WindowingMode.FULLSCREEN:
                return "Fullscreen";
            case WindowingMode.PICTURE_IN_PICTURE:
                return "PictureInPicture";
            case WindowingMode.DESKTOP_WINDOW:
                return "DesktopWindow";
            case WindowingMode.MULTI_WINDOW:
                return "DefaultMultiWindow";
            default:
                return "UNKNOWN";
        }
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

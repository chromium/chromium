// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.HashSet;
import java.util.Set;

/** Utility class for recording histograms for multi-instance features. */
@NullMarked
public class MultiWindowMetricsUtils {
    private static final long CYCLE_LENGTH_MS = DateUtils.DAY_IN_MILLIS;
    public static final int INVALID_WINDOW_ID = -1;
    public static final String WINDOWING_MODE_HISTOGRAM_PREFIX = "Android.MultiWindowMode.";
    public static final String WINDOWING_MODE_HISTOGRAM_SUFFIX = ".Duration2";

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

    /**
     * Updates the activity count for a given windowing mode and starts or stops the clock for
     * tracking time spent in that mode.
     *
     * <p>This method uses the activity count to determine if there is at least one active activity
     * in the given mode. When the first activity enters a mode, it starts a timer. When the last
     * activity in a given mode is stopped, it stops the timer and records the duration.
     *
     * @param mode The {@link WindowingMode} to update.
     * @param windowId The window ID of the activity that is entering or exiting the mode.
     * @param isStarted {@code true} if an activity is entering this mode, {@code false} if it is
     *     exiting.
     */
    public static void recordWindowingMode(int mode, int windowId, boolean isStarted) {
        if (mode == WindowingMode.UNKNOWN || windowId == INVALID_WINDOW_ID) return;
        String windowIdString = Integer.toString(windowId);
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        String modeActivitiesKey =
                ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(mode);
        Set<String> modeActivities = prefs.readStringSet(modeActivitiesKey, null);

        if (modeActivities == null) {
            modeActivities = new HashSet<>();
        } else {
            // Make a mutable copy of the set, as the returned set should not be modified.
            modeActivities = new HashSet<>(modeActivities);
        }
        int oldSize = modeActivities.size();

        if (isStarted) {
            modeActivities.add(windowIdString);
        } else {
            modeActivities.remove(windowIdString);
        }

        prefs.writeStringSet(modeActivitiesKey, modeActivities);

        if (oldSize == 0 && modeActivities.size() > 0) {
            startOrStopClockForWindowingMode(mode, /* startClock= */ true);
        } else if (oldSize > 0 && modeActivities.size() == 0) {
            startOrStopClockForWindowingMode(mode, /* startClock= */ false);
        }
    }

    private static void startOrStopClockForWindowingMode(int mode, boolean startClock) {
        if (mode == WindowingMode.UNKNOWN) return;
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        String startTimeKey = ChromePreferenceKeys.MULTI_WINDOW_MODE_START_TIME.createKey(mode);
        if (startClock) {
            long currentTime = TimeUtils.elapsedRealtimeMillis();
            prefs.writeLong(startTimeKey, currentTime);
            if (!prefs.contains(ChromePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME)) {
                prefs.writeLong(
                        ChromePreferenceKeys.MULTI_WINDOW_MODE_CYCLE_START_TIME, currentTime);
            }
        } else if (prefs.contains(startTimeKey)) {
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
        long currentTime = TimeUtils.elapsedRealtimeMillis();
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
                Set<String> modeActivities =
                        prefs.readStringSet(
                                ChromePreferenceKeys.MULTI_WINDOW_MODE_ACTIVITIES.createKey(
                                        modeIndex),
                                null);
                int modeActivityCount = modeActivities == null ? 0 : modeActivities.size();
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
            assert modeDurationMs <= CYCLE_LENGTH_MS : "Duration should not exceed cycle length.";
            RecordHistogram.recordLongTimesHistogram(
                    WINDOWING_MODE_HISTOGRAM_PREFIX
                            + histogramVariant
                            + WINDOWING_MODE_HISTOGRAM_SUFFIX,
                    modeDurationMs);
        }
        // Remove the duration key for the mode.
        prefs.removeKey(modeDurationKey);
    }

    @VisibleForTesting
    static String getWindowingModeHistogramName(int mode) {
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
}

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
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Utility class for recording histograms for multi-instance features. */
@NullMarked
public class MultiWindowMetricsUtils {
    private static final long CYCLE_LENGTH_MS = DateUtils.DAY_IN_MILLIS;
    public static final int INVALID_WINDOW_ID = -1;
    public static final String WINDOWING_MODE_HISTOGRAM_PREFIX = "Android.MultiWindowMode.";
    public static final String WINDOWING_MODE_HISTOGRAM_SUFFIX = ".Duration3";

    private static boolean sIsInitialized;

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

    private static void ensureInitialized() {
        if (sIsInitialized) return;
        sIsInitialized = true;
        // Clear any orphaned in-flight activity lists and start times left on disk from a
        // previous process that terminated without receiving onStop() (e.g. crash or unexpected
        // power-off), while preserving cycleStartTime and accumulated durationMs.
        for (int modeIndex = 1; modeIndex < WindowingMode.NUM_ENTRIES; modeIndex++) {
            MultiInstancePersistentStore.writeMultiWindowModeActivities(
                    modeIndex, Collections.emptySet());
            MultiInstancePersistentStore.removeMultiWindowModeStartTime(modeIndex);
        }
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
        ensureInitialized();
        String windowIdString = Integer.toString(windowId);
        // Make a mutable copy of the set, as the returned set should not be modified.
        Set<String> modeActivities =
                new HashSet<>(MultiInstancePersistentStore.readMultiWindowModeActivities(mode));
        int oldSize = modeActivities.size();

        if (isStarted) {
            modeActivities.add(windowIdString);
        } else {
            modeActivities.remove(windowIdString);
        }

        MultiInstancePersistentStore.writeMultiWindowModeActivities(mode, modeActivities);

        if (oldSize == 0 && modeActivities.size() > 0) {
            startOrStopClockForWindowingMode(mode, /* startClock= */ true);
        } else if (oldSize > 0 && modeActivities.size() == 0) {
            startOrStopClockForWindowingMode(mode, /* startClock= */ false);
        }
    }

    private static void startOrStopClockForWindowingMode(int mode, boolean startClock) {
        if (mode == WindowingMode.UNKNOWN) return;
        if (startClock) {
            long currentTime = TimeUtils.currentTimeMillis();
            MultiInstancePersistentStore.writeMultiWindowModeStartTime(mode, currentTime);
            long cycleStartTime = MultiInstancePersistentStore.readMultiWindowModeCycleStartTime();
            if (!MultiInstancePersistentStore.containsMultiWindowModeCycleStartTime()
                    || cycleStartTime <= 0
                    || currentTime < cycleStartTime) {
                MultiInstancePersistentStore.writeMultiWindowModeCycleStartTime(currentTime);
            } else if (currentTime >= cycleStartTime + CYCLE_LENGTH_MS) {
                recordTimeSpentInWindowingMode(/* stoppedMode= */ WindowingMode.UNKNOWN);
            }
        } else if (MultiInstancePersistentStore.containsMultiWindowModeStartTime(mode)) {
            recordTimeSpentInWindowingMode(mode);
        }
    }

    /**
     * Records the time spent in a given windowing mode and rotates any elapsed cycles.
     *
     * <p>This method uses a cycling mechanism to batch histogram reports. If at least one cycle
     * (defined by {@link #CYCLE_LENGTH_MS}) has passed since the last report, it finalizes the
     * metrics for the previous cycles, records them to a histogram, and carries over any time from
     * active modes into the next cycle. If a mode is being stopped, it also updates the total
     * duration for that mode in the current cycle.
     *
     * @param stoppedMode The windowing mode being stopped, or {@link WindowingMode#UNKNOWN} if only
     *     rotating elapsed cycles when starting a mode.
     */
    private static void recordTimeSpentInWindowingMode(int stoppedMode) {
        long currentTime = TimeUtils.currentTimeMillis();
        long cycleStartTime = MultiInstancePersistentStore.readMultiWindowModeCycleStartTime();
        if (cycleStartTime <= 0) return;

        // While at least one cycle has elapsed since the current cycle start time, update the
        // running durations for each mode and record the histogram.
        while (cycleStartTime + CYCLE_LENGTH_MS <= currentTime) {
            long cycleEndTime = cycleStartTime + CYCLE_LENGTH_MS;
            long earliestNextStartTime = currentTime;
            for (int modeIndex = 1; modeIndex < WindowingMode.NUM_ENTRIES; modeIndex++) {
                Set<String> modeActivities =
                        MultiInstancePersistentStore.readMultiWindowModeActivities(modeIndex);
                int modeActivityCount = modeActivities.size();
                long modeStartTime =
                        MultiInstancePersistentStore.readMultiWindowModeStartTime(
                                modeIndex, currentTime);

                // Update the duration for the mode if it has at least one activity open or if the
                // mode is currently being stopped, provided the session started before the end of
                // this cycle.
                if (modeActivityCount > 0 || modeIndex == stoppedMode) {
                    if (modeStartTime < cycleEndTime) {
                        // In both cases, we can safely assume that the mode was active until the
                        // end of the current cycle.
                        long durationMs =
                                MultiInstancePersistentStore.readMultiWindowModeDurationMs(
                                        modeIndex);
                        long clampedStartTime = Math.max(modeStartTime, cycleStartTime);
                        durationMs =
                                Math.min(
                                        CYCLE_LENGTH_MS,
                                        durationMs + Math.max(0L, cycleEndTime - clampedStartTime));
                        MultiInstancePersistentStore.writeMultiWindowModeDurationMs(
                                modeIndex, durationMs);
                        // Update the start time of the mode to the end of the current cycle because
                        // at this point, it is still considered active.
                        MultiInstancePersistentStore.writeMultiWindowModeStartTime(
                                modeIndex, cycleEndTime);
                        modeStartTime = cycleEndTime;
                    }
                    earliestNextStartTime = Math.min(earliestNextStartTime, modeStartTime);
                }

                recordWindowingModeHistogram(modeIndex);
            }

            // Fast-forward across any subsequent elapsed cycles where no activities were active.
            if (earliestNextStartTime > cycleEndTime) {
                long emptyCycles = (earliestNextStartTime - cycleEndTime) / CYCLE_LENGTH_MS;
                cycleEndTime += emptyCycles * CYCLE_LENGTH_MS;
            }

            // Update the cycle start time to the end of the current cycle. This will be used as the
            // start time for the next cycle.
            MultiInstancePersistentStore.writeMultiWindowModeCycleStartTime(cycleEndTime);
            cycleStartTime = cycleEndTime;
        }

        if (stoppedMode == WindowingMode.UNKNOWN) return;

        // Update the duration for the mode that is being stopped and remove the start time key.
        long modeStartTime =
                MultiInstancePersistentStore.readMultiWindowModeStartTime(stoppedMode, currentTime);
        long durationMs = MultiInstancePersistentStore.readMultiWindowModeDurationMs(stoppedMode);
        long clampedStartTime = Math.max(modeStartTime, cycleStartTime);
        durationMs =
                Math.min(
                        CYCLE_LENGTH_MS, durationMs + Math.max(0L, currentTime - clampedStartTime));
        MultiInstancePersistentStore.writeMultiWindowModeDurationMs(stoppedMode, durationMs);
        // Remove the start time key as we are done tracking the duration.
        MultiInstancePersistentStore.removeMultiWindowModeStartTime(stoppedMode);
    }

    private static void recordWindowingModeHistogram(int mode) {
        long modeDurationMs =
                Math.min(
                        CYCLE_LENGTH_MS,
                        MultiInstancePersistentStore.readMultiWindowModeDurationMs(mode));
        String histogramVariant = getWindowingModeHistogramName(mode);
        if (modeDurationMs > 0) {
            RecordHistogram.recordCustomTimesHistogram(
                    WINDOWING_MODE_HISTOGRAM_PREFIX
                            + histogramVariant
                            + WINDOWING_MODE_HISTOGRAM_SUFFIX,
                    modeDurationMs,
                    /* min= */ 1,
                    /* max= */ CYCLE_LENGTH_MS,
                    /* numBuckets= */ 50);
        }
        // Remove the duration key for the mode.
        MultiInstancePersistentStore.removeMultiWindowModeDurationMs(mode);
    }

    static void resetForTesting() {
        sIsInitialized = false;
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

    /* package */ static void recordNameWindowUserAction(@NameWindowDialogSource int source) {
        switch (source) {
            case NameWindowDialogSource.WINDOW_MANAGER:
                RecordUserAction.record("Android.WindowManager.NameWindow");
                break;
            case NameWindowDialogSource.TAB_STRIP:
                RecordUserAction.record("Android.TabStripMenu.NameWindow");
                break;
            case NameWindowDialogSource.APP_MENU:
                break;
            default:
                assert false : "Unexpected @NameWindowDialogSource.";
                break;
        }
    }

    /* package */ static void recordSaveWindowNameUserAction(@NameWindowDialogSource int source) {
        switch (source) {
            case NameWindowDialogSource.WINDOW_MANAGER:
                RecordUserAction.record("Android.WindowManager.SaveWindowName");
                break;
            case NameWindowDialogSource.TAB_STRIP:
                RecordUserAction.record("Android.TabStripMenu.SaveWindowName");
                break;
            case NameWindowDialogSource.APP_MENU:
                break;
            default:
                assert false : "Unexpected @NameWindowDialogSource.";
                break;
        }
    }

    /* package */ static void recordChangeWindowNameUserAction(@NameWindowDialogSource int source) {
        switch (source) {
            case NameWindowDialogSource.WINDOW_MANAGER:
                RecordUserAction.record("Android.WindowManager.ChangeWindowName");
                break;
            case NameWindowDialogSource.TAB_STRIP:
                RecordUserAction.record("Android.TabStripMenu.ChangeWindowName");
                break;
            case NameWindowDialogSource.APP_MENU:
                break;
            default:
                assert false : "Unexpected @NameWindowDialogSource.";
                break;
        }
    }
}

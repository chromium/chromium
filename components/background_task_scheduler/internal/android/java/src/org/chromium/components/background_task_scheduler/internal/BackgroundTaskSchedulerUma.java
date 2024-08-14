// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.content.SharedPreferences;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerExternalUma;

import java.util.HashSet;
import java.util.Set;

/** Helper class to report UMA. */
public class BackgroundTaskSchedulerUma extends BackgroundTaskSchedulerExternalUma {
    static final String KEY_CACHED_UMA = "bts_cached_uma";

    private static BackgroundTaskSchedulerUma sInstance;

    private static class CachedUmaEntry {
        private static final String SEPARATOR = ":";
        private String mEvent;
        private int mValue;
        private int mCount;

        /**
         * Parses a cached UMA entry from a string.
         *
         * @param entry A serialized entry from preferences store.
         * @return A parsed CachedUmaEntry object, or <c>null</c> if parsing failed.
         */
        public static CachedUmaEntry parseEntry(String entry) {
            if (entry == null) return null;

            String[] entryParts = entry.split(SEPARATOR);
            if (entryParts.length != 3
                    || entryParts[0].isEmpty()
                    || entryParts[1].isEmpty()
                    || entryParts[2].isEmpty()) {
                return null;
            }
            int value = -1;
            int count = -1;
            try {
                value = Integer.parseInt(entryParts[1]);
                count = Integer.parseInt(entryParts[2]);
            } catch (NumberFormatException e) {
                return null;
            }
            return new CachedUmaEntry(entryParts[0], value, count);
        }

        /** Returns a string for partial matching of the prefs entry. */
        public static String getStringForPartialMatching(String event, int value) {
            return event + SEPARATOR + value + SEPARATOR;
        }

        public CachedUmaEntry(String event, int value, int count) {
            mEvent = event;
            mValue = value;
            mCount = count;
        }

        /** Converts cached UMA entry to a string in format: EVENT:VALUE:COUNT. */
        @Override
        public String toString() {
            return mEvent + SEPARATOR + mValue + SEPARATOR + mCount;
        }

        /** Gets the name of the event (UMA). */
        public String getEvent() {
            return mEvent;
        }

        /** Gets the value of the event (concrete value of the enum). */
        public int getValue() {
            return mValue;
        }

        /** Gets the count of events that happened. */
        public int getCount() {
            return mCount;
        }

        /** Increments the count of the event. */
        public void increment() {
            mCount++;
        }
    }

    public static BackgroundTaskSchedulerUma getInstance() {
        if (sInstance == null) {
            sInstance = new BackgroundTaskSchedulerUma();
        }
        return sInstance;
    }

    public static void setInstanceForTesting(BackgroundTaskSchedulerUma instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    /** Reports metrics for task scheduling and whether it was successful. */
    public void reportTaskScheduled(int taskId, boolean success) {
        if (success) {
            cacheEvent(
                    "Android.BackgroundTaskScheduler.TaskScheduled.Success",
                    toUmaEnumValueFromTaskId(taskId));
        } else {
            cacheEvent(
                    "Android.BackgroundTaskScheduler.TaskScheduled.Failure",
                    toUmaEnumValueFromTaskId(taskId));
        }
    }

    /** Reports metrics for creating an exact tasks. */
    public void reportExactTaskCreated(int taskId) {
        cacheEvent(
                "Android.BackgroundTaskScheduler.ExactTaskCreated",
                toUmaEnumValueFromTaskId(taskId));
    }

    /** Reports metrics for task scheduling with the expiration feature activated. */
    public void reportTaskCreatedAndExpirationState(int taskId, boolean expires) {
        if (expires) {
            cacheEvent(
                    "Android.BackgroundTaskScheduler.TaskCreated.WithExpiration",
                    toUmaEnumValueFromTaskId(taskId));
        } else {
            cacheEvent(
                    "Android.BackgroundTaskScheduler.TaskCreated.WithoutExpiration",
                    toUmaEnumValueFromTaskId(taskId));
        }
    }

    /** Reports metrics for not starting a task because of expiration. */
    public void reportTaskExpired(int taskId) {
        cacheEvent("Android.BackgroundTaskScheduler.TaskExpired", toUmaEnumValueFromTaskId(taskId));
    }

    /** Reports metrics for task canceling. */
    public void reportTaskCanceled(int taskId) {
        cacheEvent(
                "Android.BackgroundTaskScheduler.TaskCanceled", toUmaEnumValueFromTaskId(taskId));
    }

    /** Reports metrics for starting a task. */
    public void reportTaskStarted(int taskId) {
        cacheEvent("Android.BackgroundTaskScheduler.TaskStarted", toUmaEnumValueFromTaskId(taskId));
    }

    /** Reports metrics for stopping a task. */
    public void reportTaskStopped(int taskId) {
        cacheEvent("Android.BackgroundTaskScheduler.TaskStopped", toUmaEnumValueFromTaskId(taskId));
    }

    /** Reports metrics for rescheduling a task. */
    public void reportTaskRescheduled() {
        cacheEvent("Android.BackgroundTaskScheduler.TaskRescheduled", 0);
    }

    /** Reports metrics for setting a notification. */
    public void reportNotificationWasSet(int taskId, long taskDurationMs) {
        RecordHistogram.recordCustomTimesHistogram(
                "Android.BackgroundTaskScheduler.SetNotification."
                        + getHistogramPatternForTaskId(taskId),
                taskDurationMs,
                1,
                DateUtils.DAY_IN_MILLIS,
                50);
    }

    @Override
    public void reportTaskFinished(int taskId, long taskDurationMs) {
        cacheEvent(
                "Android.BackgroundTaskScheduler.TaskFinished2", toUmaEnumValueFromTaskId(taskId));
        RecordHistogram.recordCustomTimesHistogram(
                "Android.BackgroundTaskScheduler.TaskFinished."
                        + getHistogramPatternForTaskId(taskId),
                taskDurationMs,
                1,
                DateUtils.DAY_IN_MILLIS,
                50);
    }

    @Override
    public void reportTaskStartedNative(int taskId) {
        int umaEnumValue = toUmaEnumValueFromTaskId(taskId);
        cacheEvent("Android.BackgroundTaskScheduler.TaskLoadedNative", umaEnumValue);
    }

    @Override
    public void reportStartupMode(int startupMode) {
        // We don't record full browser's warm startup since most of the full browser warm startup
        // don't even reach here.
        if (startupMode < 0) return;

        cacheEvent("Servicification.Startup3", startupMode);
    }

    /** Method that actually invokes histogram recording. Extracted for testing. */
    @VisibleForTesting
    void recordEnumeratedHistogram(String histogram, int value, int maxCount) {
        RecordHistogram.recordEnumeratedHistogram(histogram, value, maxCount);
    }

    /** Records histograms for cached stats. Should only be called when native is initialized. */
    public void flushStats() {
        assertNativeIsLoaded();
        ThreadUtils.assertOnUiThread();

        Set<String> cachedUmaStrings = getCachedUmaEntries(ContextUtils.getAppSharedPreferences());

        for (String cachedUmaString : cachedUmaStrings) {
            CachedUmaEntry entry = CachedUmaEntry.parseEntry(cachedUmaString);
            if (entry == null) continue;
            for (int i = 0; i < entry.getCount(); i++) {
                recordEnumeratedHistogram(
                        entry.getEvent(), entry.getValue(), BACKGROUND_TASK_COUNT);
            }
        }

        // Once all metrics are reported, we can simply remove the shared preference key.
        removeCachedStats();
    }

    /** Removes all of the cached stats without reporting. */
    public void removeCachedStats() {
        ThreadUtils.assertOnUiThread();
        ContextUtils.getAppSharedPreferences().edit().remove(KEY_CACHED_UMA).apply();
    }

    /** Caches the event to be reported through UMA in shared preferences. */
    @VisibleForTesting
    void cacheEvent(String event, int value) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        Set<String> cachedUmaStrings = getCachedUmaEntries(prefs);
        String partialMatch = CachedUmaEntry.getStringForPartialMatching(event, value);

        String existingEntry = null;
        for (String cachedUmaString : cachedUmaStrings) {
            if (cachedUmaString.startsWith(partialMatch)) {
                existingEntry = cachedUmaString;
                break;
            }
        }

        Set<String> setToWriteBack = new HashSet<>(cachedUmaStrings);
        CachedUmaEntry entry = null;
        if (existingEntry != null) {
            entry = CachedUmaEntry.parseEntry(existingEntry);
            if (entry == null) {
                entry = new CachedUmaEntry(event, value, 1);
            }
            setToWriteBack.remove(existingEntry);
            entry.increment();
        } else {
            entry = new CachedUmaEntry(event, value, 1);
        }

        setToWriteBack.add(entry.toString());
        updateCachedUma(prefs, setToWriteBack);
    }

    @VisibleForTesting
    static Set<String> getCachedUmaEntries(SharedPreferences prefs) {
        Set<String> cachedUmaEntries = prefs.getStringSet(KEY_CACHED_UMA, new HashSet<>());
        return sanitizeEntrySet(cachedUmaEntries);
    }

    @VisibleForTesting
    static void updateCachedUma(SharedPreferences prefs, Set<String> cachedUma) {
        ThreadUtils.assertOnUiThread();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet(KEY_CACHED_UMA, sanitizeEntrySet(cachedUma));
        editor.apply();
    }

    void assertNativeIsLoaded() {
        assert LibraryLoader.getInstance().isInitialized();
    }

    private static Set<String> sanitizeEntrySet(Set<String> set) {
        if (set != null && set.contains(null)) {
            set.remove(null);
        }
        return set;
    }
}

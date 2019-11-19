// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;

import java.util.HashSet;
import java.util.Set;

class BackgroundTaskSchedulerUma {
    // BackgroundTaskId defined in tools/metrics/histograms/enums.xml
    static final int BACKGROUND_TASK_TEST = 0;
    static final int BACKGROUND_TASK_OMAHA = 1;
    static final int BACKGROUND_TASK_GCM = 2;
    static final int BACKGROUND_TASK_NOTIFICATIONS = 3;
    static final int BACKGROUND_TASK_WEBVIEW_MINIDUMP = 4;
    static final int BACKGROUND_TASK_CHROME_MINIDUMP = 5;
    static final int BACKGROUND_TASK_OFFLINE_PAGES = 6;
    static final int BACKGROUND_TASK_OFFLINE_PREFETCH = 7;
    static final int BACKGROUND_TASK_DOWNLOAD_SERVICE = 8;
    static final int BACKGROUND_TASK_DOWNLOAD_CLEANUP = 9;
    static final int BACKGROUND_TASK_WEBVIEW_VARIATIONS = 10;
    static final int BACKGROUND_TASK_OFFLINE_CONTENT_NOTIFICATION = 11;
    static final int BACKGROUND_TASK_WEBAPK_UPDATE = 12;
    static final int BACKGROUND_TASK_DOWNLOAD_RESUMPTION = 13;
    static final int BACKGROUND_TASK_FEED_REFRESH = 14;
    static final int BACKGROUND_TASK_COMPONENT_UPDATE = 15;
    static final int BACKGROUND_TASK_DEPRECATED_EXPLORE_SITES_REFRESH = 16;
    static final int BACKGROUND_TASK_EXPLORE_SITES_REFRESH = 17;
    static final int BACKGROUND_TASK_DOWNLOAD_AUTO_RESUMPTION = 18;
    static final int BACKGROUND_TASK_ONE_SHOT_SYNC_WAKE_UP = 19;
    static final int BACKGROUND_TASK_NOTIFICATION_SCHEDULER = 20;
    static final int BACKGROUND_TASK_NOTIFICATION_TRIGGER = 21;
    static final int BACKGROUND_TASK_PERIODIC_SYNC_WAKE_UP = 22;
    // Keep this one at the end and increment appropriately when adding new tasks.
    static final int BACKGROUND_TASK_COUNT = 23;

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
            if (entryParts.length != 3 || entryParts[0].isEmpty() || entryParts[1].isEmpty()
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

    @VisibleForTesting
    public static void setInstanceForTesting(BackgroundTaskSchedulerUma instance) {
        sInstance = instance;
    }

    /** Reports metrics for task scheduling and whether it was successful. */
    public void reportTaskScheduled(int taskId, boolean success) {
        if (success) {
            cacheEvent("Android.BackgroundTaskScheduler.TaskScheduled.Success",
                    toUmaEnumValueFromTaskId(taskId));
        } else {
            cacheEvent("Android.BackgroundTaskScheduler.TaskScheduled.Failure",
                    toUmaEnumValueFromTaskId(taskId));
        }
    }

    /** Reports metrics for creating an exact tasks. */
    public void reportExactTaskCreated(int taskId) {
        cacheEvent("Android.BackgroundTaskScheduler.ExactTaskCreated",
                toUmaEnumValueFromTaskId(taskId));
    }

    /** Reports metrics for task scheduling with the expiration feature activated. */
    public void reportTaskCreatedAndExpirationState(int taskId, boolean expires) {
        if (expires) {
            cacheEvent("Android.BackgroundTaskScheduler.TaskCreated.WithExpiration",
                    toUmaEnumValueFromTaskId(taskId));
        } else {
            cacheEvent("Android.BackgroundTaskScheduler.TaskCreated.WithoutExpiration",
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

    /** Reports metrics for migrating scheduled tasks to Protocol Buffer data format. */
    public void reportMigrationToProto(int taskId) {
        cacheEvent("Android.BackgroundTaskScheduler.MigrationToProto",
                toUmaEnumValueFromTaskId(taskId));
    }

    /**
     * Reports metrics for when a NativeBackgroundTask loads the native library.
     * @param taskId An id from {@link TaskIds}.
     * @param serviceManagerOnlyMode Whether the task will start native in Service Manager Only Mode
     *                              (Reduced Mode) instead of Full Browser Mode.
     */
    public void reportTaskStartedNative(int taskId, boolean serviceManagerOnlyMode) {
        int umaEnumValue = toUmaEnumValueFromTaskId(taskId);
        cacheEvent("Android.BackgroundTaskScheduler.TaskLoadedNative", umaEnumValue);
        if (serviceManagerOnlyMode) {
            cacheEvent(
                    "Android.BackgroundTaskScheduler.TaskLoadedNative.ReducedMode", umaEnumValue);
        } else {
            cacheEvent(
                    "Android.BackgroundTaskScheduler.TaskLoadedNative.FullBrowser", umaEnumValue);
        }
    }

    /**
     * Report metrics for starting a NativeBackgroundTask. This does not consider tasks that are
     * short-circuited before any work is done.
     * @param taskId An id from {@link TaskIds}.
     * @param serviceManagerOnlyMode Whether the task will run in Service Manager Only Mode (Reduced
     *                               Mode) instead of Full Browser Mode.
     */
    public void reportNativeTaskStarted(int taskId, boolean serviceManagerOnlyMode) {
        int umaEnumValue = toUmaEnumValueFromTaskId(taskId);
        cacheEvent("Android.NativeBackgroundTask.TaskStarted", umaEnumValue);
        if (serviceManagerOnlyMode) {
            cacheEvent("Android.NativeBackgroundTask.TaskStarted.ReducedMode", umaEnumValue);
        } else {
            cacheEvent("Android.NativeBackgroundTask.TaskStarted.FullBrowser", umaEnumValue);
        }
    }

    /**
     * Reports metrics that a NativeBackgroundTask has been finished cleanly (i.e., no unexpected
     * exits because of chrome crash or OOM). This includes tasks that have been stopped due to
     * timeout.
     * @param taskId An id from {@link TaskIds}.
     * @param serviceManagerOnlyMode Whether the task will run in Service Manager Only Mode (Reduced
     *                               Mode) instead of Full Browser Mode.
     */
    public void reportNativeTaskFinished(int taskId, boolean serviceManagerOnlyMode) {
        int umaEnumValue = toUmaEnumValueFromTaskId(taskId);
        cacheEvent("Android.NativeBackgroundTask.TaskFinished", umaEnumValue);
        if (serviceManagerOnlyMode) {
            cacheEvent("Android.NativeBackgroundTask.TaskFinished.ReducedMode", umaEnumValue);
        } else {
            cacheEvent("Android.NativeBackgroundTask.TaskFinished.FullBrowser", umaEnumValue);
        }
    }

    /**
     * Reports metrics of how Chrome is launched, either in ServiceManager only mode or as full
     * browser, as well as either cold start or warm start.
     * See {@link org.chromium.content.browser.ServicificationStartupUma} for more details.
     * @param startupMode Chrome's startup mode.
     */
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
    static int toUmaEnumValueFromTaskId(int taskId) {
        switch (taskId) {
            case TaskIds.TEST:
                return BACKGROUND_TASK_TEST;
            case TaskIds.OMAHA_JOB_ID:
                return BACKGROUND_TASK_OMAHA;
            case TaskIds.GCM_BACKGROUND_TASK_JOB_ID:
                return BACKGROUND_TASK_GCM;
            case TaskIds.NOTIFICATION_SERVICE_JOB_ID:
                return BACKGROUND_TASK_NOTIFICATIONS;
            case TaskIds.WEBVIEW_MINIDUMP_UPLOADING_JOB_ID:
                return BACKGROUND_TASK_WEBVIEW_MINIDUMP;
            case TaskIds.CHROME_MINIDUMP_UPLOADING_JOB_ID:
                return BACKGROUND_TASK_CHROME_MINIDUMP;
            case TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID:
                return BACKGROUND_TASK_OFFLINE_PAGES;
            case TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID:
                return BACKGROUND_TASK_OFFLINE_PREFETCH;
            case TaskIds.DOWNLOAD_SERVICE_JOB_ID:
                return BACKGROUND_TASK_DOWNLOAD_SERVICE;
            case TaskIds.DOWNLOAD_CLEANUP_JOB_ID:
                return BACKGROUND_TASK_DOWNLOAD_CLEANUP;
            case TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID:
                return BACKGROUND_TASK_DOWNLOAD_AUTO_RESUMPTION;
            case TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID:
                return BACKGROUND_TASK_WEBVIEW_VARIATIONS;
            case TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID:
                return BACKGROUND_TASK_OFFLINE_CONTENT_NOTIFICATION;
            case TaskIds.WEBAPK_UPDATE_JOB_ID:
                return BACKGROUND_TASK_WEBAPK_UPDATE;
            case TaskIds.DOWNLOAD_RESUMPTION_JOB_ID:
                return BACKGROUND_TASK_DOWNLOAD_RESUMPTION;
            case TaskIds.FEED_REFRESH_JOB_ID:
                return BACKGROUND_TASK_FEED_REFRESH;
            case TaskIds.COMPONENT_UPDATE_JOB_ID:
                return BACKGROUND_TASK_COMPONENT_UPDATE;
            case TaskIds.DEPRECATED_EXPLORE_SITES_REFRESH_JOB_ID:
                return BACKGROUND_TASK_DEPRECATED_EXPLORE_SITES_REFRESH;
            case TaskIds.EXPLORE_SITES_REFRESH_JOB_ID:
                return BACKGROUND_TASK_EXPLORE_SITES_REFRESH;
            case TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID:
                return BACKGROUND_TASK_ONE_SHOT_SYNC_WAKE_UP;
            case TaskIds.NOTIFICATION_SCHEDULER_JOB_ID:
                return BACKGROUND_TASK_NOTIFICATION_SCHEDULER;
            case TaskIds.NOTIFICATION_TRIGGER_JOB_ID:
                return BACKGROUND_TASK_NOTIFICATION_TRIGGER;
            case TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID:
                return BACKGROUND_TASK_PERIODIC_SYNC_WAKE_UP;
            default:
                assert false;
        }
        // Returning a value that is not expected to ever be reported.
        return BACKGROUND_TASK_TEST;
    }

    @VisibleForTesting
    static Set<String> getCachedUmaEntries(SharedPreferences prefs) {
        return prefs.getStringSet(KEY_CACHED_UMA, new HashSet<String>(1));
    }

    @VisibleForTesting
    static void updateCachedUma(SharedPreferences prefs, Set<String> cachedUma) {
        ThreadUtils.assertOnUiThread();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putStringSet(KEY_CACHED_UMA, cachedUma);
        editor.apply();
    }

    void assertNativeIsLoaded() {
        assert LibraryLoader.getInstance().isInitialized();
    }
}

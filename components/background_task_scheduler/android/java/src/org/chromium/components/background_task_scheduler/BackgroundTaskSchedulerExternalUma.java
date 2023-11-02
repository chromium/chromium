// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import androidx.annotation.VisibleForTesting;

/**
 * Helper class to allow external code (typically Chrome-specific BackgroundTaskScheduler code) to
 * report UMA.
 */
public abstract class BackgroundTaskSchedulerExternalUma {
    // BackgroundTaskId defined in tools/metrics/histograms/enums.xml
    public static final int BACKGROUND_TASK_NOT_FOUND = -1;
    public static final int BACKGROUND_TASK_TEST = 0;
    public static final int BACKGROUND_TASK_OMAHA = 1;
    public static final int BACKGROUND_TASK_GCM = 2;
    public static final int BACKGROUND_TASK_NOTIFICATIONS = 3;
    public static final int BACKGROUND_TASK_WEBVIEW_MINIDUMP = 4;
    public static final int BACKGROUND_TASK_CHROME_MINIDUMP = 5;
    public static final int BACKGROUND_TASK_OFFLINE_PAGES = 6;
    public static final int BACKGROUND_TASK_OFFLINE_PREFETCH = 7;
    public static final int BACKGROUND_TASK_DOWNLOAD_SERVICE = 8;
    public static final int BACKGROUND_TASK_DOWNLOAD_CLEANUP = 9;
    public static final int BACKGROUND_TASK_WEBVIEW_VARIATIONS = 10;
    public static final int BACKGROUND_TASK_OFFLINE_CONTENT_NOTIFICATION = 11;
    public static final int BACKGROUND_TASK_WEBAPK_UPDATE = 12;
    public static final int BACKGROUND_TASK_DEPRECATED_DOWNLOAD_RESUMPTION = 13;
    public static final int BACKGROUND_TASK_FEED_REFRESH = 14;
    public static final int BACKGROUND_TASK_COMPONENT_UPDATE = 15;
    public static final int BACKGROUND_TASK_DEPRECATED_EXPLORE_SITES_REFRESH = 16;
    public static final int BACKGROUND_TASK_EXPLORE_SITES_REFRESH = 17;
    public static final int BACKGROUND_TASK_DOWNLOAD_AUTO_RESUMPTION = 18;
    public static final int BACKGROUND_TASK_ONE_SHOT_SYNC_WAKE_UP = 19;
    public static final int BACKGROUND_TASK_NOTIFICATION_SCHEDULER = 20;
    public static final int BACKGROUND_TASK_NOTIFICATION_TRIGGER = 21;
    public static final int BACKGROUND_TASK_PERIODIC_SYNC_WAKE_UP = 22;
    public static final int BACKGROUND_TASK_QUERY_TILE = 23;
    public static final int BACKGROUND_TASK_FEEDV2_REFRESH = 24;
    public static final int BACKGROUND_TASK_DOWNLOAD_LATER = 25;
    public static final int BACKGROUND_TASK_OFFLINE_MEASUREMENTS = 26;
    public static final int BACKGROUND_TASK_WEBVIEW_COMPONENT_UPDATE = 27;
    public static final int BACKGROUND_TASK_ATTRIBUTION_PROVIDER_FLUSH = 28;
    // Keep this one at the end and increment appropriately when adding new tasks.
    public static final int BACKGROUND_TASK_COUNT = 29;

    protected BackgroundTaskSchedulerExternalUma() {}

    /**
     * Reports metrics for when a NativeBackgroundTask loads the native library.
     * @param taskId An id from {@link TaskIds}.
     * @param minimalBrowserMode Whether the task will start native in Minimal Browser Mode
     *                              (Reduced Mode) instead of Full Browser Mode.
     */
    public abstract void reportTaskStartedNative(int taskId, boolean minimalBrowserMode);

    /**
     * Report metrics for starting a NativeBackgroundTask. This does not consider tasks that are
     * short-circuited before any work is done.
     * @param taskId An id from {@link TaskIds}.
     * @param minimalBrowserMode Whether the task will run in Minimal Browser Mode (Reduced
     *                               Mode) instead of Full Browser Mode.
     */
    public abstract void reportNativeTaskStarted(int taskId, boolean minimalBrowserMode);

    /**
     * Reports metrics that a NativeBackgroundTask has been finished cleanly (i.e., no unexpected
     * exits because of chrome crash or OOM). This includes tasks that have been stopped due to
     * timeout.
     * @param taskId An id from {@link TaskIds}.
     * @param minimalBrowserMode Whether the task will run in Minimal Browser Mode (Reduced
     *                               Mode) instead of Full Browser Mode.
     */
    public abstract void reportNativeTaskFinished(int taskId, boolean minimalBrowserMode);

    /**
     * Reports metrics of how Chrome is launched, either in minimal browser mode or as full
     * browser, as well as either cold start or warm start.
     * See {@link org.chromium.content.browser.ServicificationStartupUma} for more details.
     * @param startupMode Chrome's startup mode.
     */
    public abstract void reportStartupMode(int startupMode);

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public static int toUmaEnumValueFromTaskId(int taskId) {
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
            case TaskIds.DOWNLOAD_LATER_JOB_ID:
                return BACKGROUND_TASK_DOWNLOAD_LATER;
            case TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID:
                return BACKGROUND_TASK_WEBVIEW_VARIATIONS;
            case TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID:
                return BACKGROUND_TASK_OFFLINE_CONTENT_NOTIFICATION;
            case TaskIds.WEBAPK_UPDATE_JOB_ID:
                return BACKGROUND_TASK_WEBAPK_UPDATE;
            case TaskIds.DEPRECATED_DOWNLOAD_RESUMPTION_JOB_ID:
                return BACKGROUND_TASK_DEPRECATED_DOWNLOAD_RESUMPTION;
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
            case TaskIds.QUERY_TILE_JOB_ID:
                return BACKGROUND_TASK_QUERY_TILE;
            case TaskIds.FEEDV2_REFRESH_JOB_ID:
                return BACKGROUND_TASK_FEEDV2_REFRESH;
            case TaskIds.WEBVIEW_COMPONENT_UPDATE_JOB_ID:
                return BACKGROUND_TASK_WEBVIEW_COMPONENT_UPDATE;
        }
        // Returning a value that is not expected to ever be reported.
        return BACKGROUND_TASK_NOT_FOUND;
    }

    /**
     * Keep this in sync with TaskType variant in
     * //tools/metrics/histograms/metadata/android/histograms.xml.
     * @return The histogram pattern to be used for the given {@code taskId}.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public String getHistogramPatternForTaskId(int taskId) {
        switch (taskId) {
            case TaskIds.TEST:
                return "Test";
            case TaskIds.OMAHA_JOB_ID:
                return "Omaha";
            case TaskIds.GCM_BACKGROUND_TASK_JOB_ID:
                return "Gcm";
            case TaskIds.NOTIFICATION_SERVICE_JOB_ID:
                return "NotificationService";
            case TaskIds.WEBVIEW_MINIDUMP_UPLOADING_JOB_ID:
                return "WebviewMinidumpUploading";
            case TaskIds.CHROME_MINIDUMP_UPLOADING_JOB_ID:
                return "ChromeMinidumpUploading";
            case TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID:
                return "OfflinePages";
            case TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID:
                return "OfflinePagesPrefetch";
            case TaskIds.DOWNLOAD_SERVICE_JOB_ID:
                return "DownloadService";
            case TaskIds.DOWNLOAD_CLEANUP_JOB_ID:
                return "DownloadCleanup";
            case TaskIds.DOWNLOAD_AUTO_RESUMPTION_JOB_ID:
                return "DownloadAutoResumption";
            case TaskIds.DOWNLOAD_LATER_JOB_ID:
                return "DownloadLater";
            case TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID:
                return "WebviewVariationsSeedFetch";
            case TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID:
                return "OfflinePagesPrefetchNotification";
            case TaskIds.WEBAPK_UPDATE_JOB_ID:
                return "WebApkUpdate";
            case TaskIds.DEPRECATED_DOWNLOAD_RESUMPTION_JOB_ID:
                return "DeprecatedDownloadResumption";
            case TaskIds.FEED_REFRESH_JOB_ID:
                return "FeedRefresh";
            case TaskIds.COMPONENT_UPDATE_JOB_ID:
                return "ComponentUpdate";
            case TaskIds.DEPRECATED_EXPLORE_SITES_REFRESH_JOB_ID:
                return "DeprecatedExploreSitesRefresh";
            case TaskIds.EXPLORE_SITES_REFRESH_JOB_ID:
                return "ExploreSitesRefresh";
            case TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID:
                return "BackgroundSyncOneShot";
            case TaskIds.NOTIFICATION_SCHEDULER_JOB_ID:
                return "NotificationScheduler";
            case TaskIds.NOTIFICATION_TRIGGER_JOB_ID:
                return "NotificationTrigger";
            case TaskIds.PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID:
                return "PeriodicBackgroundSyncChromeWakeup";
            case TaskIds.QUERY_TILE_JOB_ID:
                return "QueryTile";
            case TaskIds.FEEDV2_REFRESH_JOB_ID:
                return "FeedV2Refresh";
            case TaskIds.WEBVIEW_COMPONENT_UPDATE_JOB_ID:
                return "WebviewComponentUpdate";
        }
        assert false;
        return null;
    }
}

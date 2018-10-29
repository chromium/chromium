// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

/**
 * This class lists all the unique task IDs used around in Chromium. These are listed here to ensure
 * that there is no overlap of task IDs between different users of the BackgroundTaskScheduler.
 */
public final class TaskIds {
    // When adding your job id to the list below, remember to make a corresponding update to the
    // BackgroundTaskSchedulerUma#toUmaEnumValueFromTaskId(int) method.
    public static final int TEST = 0x00008378;
    public static final int OMAHA_JOB_ID = 0x00011684;

    public static final int GCM_BACKGROUND_TASK_JOB_ID = 1;
    public static final int NOTIFICATION_SERVICE_JOB_ID = 21;
    public static final int WEBVIEW_MINIDUMP_UPLOADING_JOB_ID = 42;
    public static final int CHROME_MINIDUMP_UPLOADING_JOB_ID = 43;
    public static final int OFFLINE_PAGES_BACKGROUND_JOB_ID = 77;
    public static final int OFFLINE_PAGES_PREFETCH_JOB_ID = 78;
    public static final int OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID = 79;
    public static final int DOWNLOAD_SERVICE_JOB_ID = 53;
    public static final int DOWNLOAD_CLEANUP_JOB_ID = 54;
    public static final int WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID = 83;
    public static final int WEBAPK_UPDATE_JOB_ID = 91;
    public static final int DOWNLOAD_RESUMPTION_JOB_ID = 55;
    public static final int FEED_REFRESH_JOB_ID = 22;
    public static final int COMPONENT_UPDATE_JOB_ID = 2;
    public static final int EXPLORE_SITES_REFRESH_JOB_ID = 100;

    private TaskIds() {}
}

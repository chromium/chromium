// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_IDS_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_IDS_H_

namespace background_task {

// This enum lists all the unique task IDs used around in Chromium. These are
// listed here to ensure that there is no overlap of task IDs between different
// users of the BackgroundTaskScheduler.
// When adding your job id to the list below, remember to make a corresponding
// update to the BackgroundTaskSchedulerExternalUma for metrics.
// Also, if the new task id is related to a BackgroundTask class in
// //chrome, remember to update
// ChromeBackgroundTaskFactory#createBackgroundTaskFromTaskId in java.
// If the task is a native task, also update
// ChromeBackgroundTaskFactory::GetNativeBackgroundTaskFromTaskId. Id from
// 111000000 to 111999999 are reserved for internal usage. A Java counterpart
// will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.background_task_scheduler)
enum class TaskIds {
  TEST = 0x00008378,
  OMAHA_JOB_ID = 0x00011684,
  GCM_BACKGROUND_TASK_JOB_ID = 1,
  NOTIFICATION_SERVICE_JOB_ID = 21,
  WEBVIEW_MINIDUMP_UPLOADING_JOB_ID = 42,
  CHROME_MINIDUMP_UPLOADING_JOB_ID = 43,
  OFFLINE_PAGES_BACKGROUND_JOB_ID = 77,
  OFFLINE_PAGES_PREFETCH_JOB_ID = 78,
  OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID = 79,
  DOWNLOAD_SERVICE_JOB_ID = 53,
  DOWNLOAD_CLEANUP_JOB_ID = 54,
  WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID = 83,
  WEBAPK_UPDATE_JOB_ID = 91,
  DEPRECATED_DOWNLOAD_RESUMPTION_JOB_ID = 55,
  DOWNLOAD_AUTO_RESUMPTION_JOB_ID = 56,
  DOWNLOAD_LATER_JOB_ID = 57,
  FEED_REFRESH_JOB_ID = 22,
  COMPONENT_UPDATE_JOB_ID = 2,
  BACKGROUND_SYNC_ONE_SHOT_JOB_ID = 102,
  NOTIFICATION_SCHEDULER_JOB_ID = 103,
  NOTIFICATION_TRIGGER_JOB_ID = 104,
  PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID = 105,
  QUERY_TILE_JOB_ID = 106,
  FEEDV2_REFRESH_JOB_ID = 107,
  WEBFEEDS_REFRESH_JOB_ID = 109,
  WEBVIEW_COMPONENT_UPDATE_JOB_ID = 110,
  ATTRIBUTION_PROVIDER_FLUSH_JOB_ID = 111,
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_IDS_H_

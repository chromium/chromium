// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_IDS_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_IDS_H_

namespace background_task {

// This enum lists all the unique task IDs used around in Chromium. These are
// listed here to ensure that there is no overlap of task IDs between different
// users of the BackgroundTaskScheduler.
// When adding your job id to the list below, update:
// 1. BackgroundTaskSchedulerExternalUma for metrics, specifically:
// toUmaEnumValueFromTaskId() and getHistogramPatternForTaskId().
// 2. Enum BackgroundTaskId in tools/metrics/histograms/enums.xml.
// 3. Variant TaskType in
// tools/metrics/histograms/metadata/android/histograms.xml.
// 4. ChromeBackgroundTaskFactory#createBackgroundTaskFromTaskId in java.
// 5. BackgroundTaskSchedulerUmaTest#testToUmaEnumValueFromTaskId for
// updated BACKGROUND_TASK_COUNT.
// 6. If the task is a native task, also update
// ChromeBackgroundTaskFactory::GetNativeBackgroundTaskFromTaskId.

// Id from 111000000 to 111999999 are reserved for internal usage. A Java
// counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.background_task_scheduler)
enum class TaskIds {
  // component: Internals>BackgroundTaskScheduler
  // team_email: clank-dev@google.com
  // owner: nyquist@chromium.org, shaktisahu@chromium.org
  TEST = 0x00008378,
  // component: Internals>Installer
  // team_email: chrome-updates-dev@chromium.org
  // owner: waffles@chromium.org
  OMAHA_JOB_ID = 0x00011684,
  // component: Services>CloudMessaging
  // team_email:
  // owner: peter@chromium.org
  GCM_BACKGROUND_TASK_JOB_ID = 1,
  // component: UI>Notifications
  // team_email: platform-capabilities@chromium.org
  // owner: peter@chromium.org
  NOTIFICATION_SERVICE_JOB_ID = 21,
  // component: UI>Notifications
  // team_email: platform-capabilities@chromium.org
  // owner: engedy@chromium.org
  NOTIFICATION_SERVICE_PRE_UNSUBSCRIBE_JOB_ID = 221,
  // component: Mobile>WebView
  // team_email: android-webview-dev@chromium.org
  // owner: boliu@chromium.org
  WEBVIEW_MINIDUMP_UPLOADING_JOB_ID = 42,
  // component: Internals>CrashReporting
  // team_email:
  // owner: wnwen@chromium.org
  CHROME_MINIDUMP_UPLOADING_JOB_ID = 43,
  // component: UI>Browser>Offline
  // team_email: offline-dev@chromium.org
  // owner: dewittj@chromium.org
  OFFLINE_PAGES_BACKGROUND_JOB_ID = 77,
  // component: UI>Browser>Downloads
  // team_email:
  // owner: qinmin@chromium.org
  DOWNLOAD_SERVICE_JOB_ID = 53,
  // component: UI>Browser>Downloads
  // team_email:
  // owner: qinmin@chromium.org
  DOWNLOAD_CLEANUP_JOB_ID = 54,
  // component: Mobile>WebView
  // team_email: android-webview-dev@chromium.org
  // owner: ntfschr@chromium.org, torne@chromium.org
  WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID = 83,
  // component: UI>Browser>WebAppInstalls
  // team_email:
  // owner: hartmanng@chromium.org
  WEBAPK_UPDATE_JOB_ID = 91,
  // component: UI>Browser>Downloads
  // team_email:
  // owner: qinmin@chromium.org
  DEPRECATED_DOWNLOAD_RESUMPTION_JOB_ID = 55,
  // component: UI>Browser>Downloads
  // team_email:
  // owner: qinmin@chromium.org
  DOWNLOAD_AUTO_RESUMPTION_JOB_ID = 56,
  // component: UI>Browser>Downloads
  // team_email:
  // owner: qinmin@chromium.org
  DOWNLOAD_LATER_JOB_ID = 57,
  // component: UI>Browser>Downloads
  // team_email:
  // owner: qinmin@chromium.org
  DOWNLOAD_AUTO_RESUMPTION_UNMETERED_JOB_ID = 58,
  // component: UI>Browser>Downloads
  // team_email:
  // owner: qinmin@chromium.org
  DOWNLOAD_AUTO_RESUMPTION_ANY_NETWORK_JOB_ID = 59,
  // component: UI>Browser>ContentSuggestions>Feed
  // team_email: feed@chromium.org
  // owner: dewittj@chromium.org
  FEED_REFRESH_JOB_ID = 22,
  // component: Internals>Installer>Components
  // team_email: chrome-updates-dev@chromium.org
  // owner: waffles@chromium.org
  COMPONENT_UPDATE_JOB_ID = 2,
  // component: Blink>BackgroundSync
  // team_email: platform-capabilities@chromium.org
  // owner: peter@chromium.org
  BACKGROUND_SYNC_ONE_SHOT_JOB_ID = 102,
  // component: UI>Notifications
  // team_email: platform-capabilities@chromium.org
  // owner: dtrainor@chromium.org
  NOTIFICATION_SCHEDULER_JOB_ID = 103,
  // component: UI>Notifications
  // team_email: platform-capabilities@chromium.org
  // owner: peter@chromium.org, dtrainor@chromium.org
  NOTIFICATION_TRIGGER_JOB_ID = 104,
  // component: Blink>BackgroundSync
  // team_email: platform-capabilities@chromium.org
  // owner: peter@chromium.org
  PERIODIC_BACKGROUND_SYNC_CHROME_WAKEUP_TASK_JOB_ID = 105,
  // component: Upboarding>QueryTiles
  // team_email: chrome-upboarding-eng@google.com
  // owner: qinmin@chromium.org, shaktisahu@chromium.org
  QUERY_TILE_JOB_ID = 106,
  // component: UI>Browser>ContentSuggestions>Feed
  // team_email: feed@chromium.org
  // owner: dewittj@chromium.org
  FEEDV2_REFRESH_JOB_ID = 107,
  // component: UI>Browser>ContentSuggestions>Feed
  // team_email: feed@chromium.org
  // owner: dewittj@chromium.org
  WEBFEEDS_REFRESH_JOB_ID = 109,
  // component: Mobile>WebView
  // team_email: android-webview-dev@chromium.org
  // owner: ntfschr@chromium.org, torne@chromium.org
  WEBVIEW_COMPONENT_UPDATE_JOB_ID = 110,
  // component: Internals>AttributionReporting
  // team_email: privacy-sandbox-dev@chromium.org
  // owner: csharrison@chromium.org
  ATTRIBUTION_PROVIDER_FLUSH_JOB_ID = 111,
  // component: UI>Settings>Privacy
  // team_email: chrome-privacy-controls@google.com
  // owner: zalmashni@google.com, rubindl@chromium.org
  SAFETY_HUB_JOB_ID = 112,
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_TASK_IDS_H_

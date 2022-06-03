// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_EXTENDED_CRASH_REPORTING_METRICS_H_
#define COMPONENTS_BROWSER_WATCHER_EXTENDED_CRASH_REPORTING_METRICS_H_

namespace browser_watcher {

// DO NOT REMOVE OR REORDER VALUES. This is logged persistently in a histogram.
enum class CollectOnCrashEvent {
  kCollectAttempt,
  kUserDataDirNotEmptyUnused,  // No longer used.
  kPathExistsUnused,           // No longer used.
  kReportExtractionSuccess,
  kPmaSetDeletedFailedUnused,  // No longer used.
  kOpenForDeleteFailedUnused,  // No longer used.
  kSuccess,
  kInMemoryAnnotationExists,
  // New values go here.
  kCollectOnCrashEventMax
};

// DO NOT REMOVE OR REORDER VALUES. This is logged persistently in a histogram.
enum class ActivityRecordEvent {
  kRecordAttempt,
  kActivityDirectoryExistsUnused,  // No longer used.
  kGotActivityPathUnused,          // No longer used.
  kGotTracker,
  kMarkDeletedUnused,          // No longer used.
  kMarkDeletedGotFileUnused,   // No longer used.
  kOpenForDeleteFailedUnused,  // No longer used.
  // New values go here.
  kActivityRecordEventMax
};

void LogCollectOnCrashEvent(CollectOnCrashEvent event);
void LogActivityRecordEvent(ActivityRecordEvent event);

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_EXTENDED_CRASH_REPORTING_METRICS_H_

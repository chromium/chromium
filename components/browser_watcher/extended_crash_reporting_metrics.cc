// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/extended_crash_reporting_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace browser_watcher {

void LogCollectOnCrashEvent(CollectOnCrashEvent event) {
  base::UmaHistogramEnumeration("ActivityTracker.CollectCrash.Event", event,
                                CollectOnCrashEvent::kCollectOnCrashEventMax);
}

void LogActivityRecordEvent(ActivityRecordEvent event) {
  base::UmaHistogramEnumeration("ActivityTracker.Record.Event", event,
                                ActivityRecordEvent::kActivityRecordEventMax);
}

}  // namespace browser_watcher

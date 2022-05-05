// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace {
const int kBytesPerKB = 1024;
}

namespace bookmarks::metrics {

void RecordTimeSinceLastScheduledSave(base::TimeDelta delta) {
  UmaHistogramLongTimes("Bookmarks.Storage.TimeSinceLastScheduledSave", delta);
}

void RecordTimeToLoadAtStartup(base::TimeDelta delta) {
  UmaHistogramTimes("Bookmarks.Storage.TimeToLoadAtStartup", delta);
}

void RecordFileSizeAtStartup(int64_t total_bytes) {
  int total_size_kb = base::saturated_cast<int>(total_bytes / kBytesPerKB);
  base::UmaHistogramCounts1M("Bookmarks.Storage.FileSizeAtStartup",
                             total_size_kb);
}

}  // namespace bookmarks::metrics
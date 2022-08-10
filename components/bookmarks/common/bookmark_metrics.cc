// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "components/bookmarks/common/url_load_stats.h"

namespace {
const int kBytesPerKB = 1024;
}

namespace bookmarks::metrics {

void RecordBookmarkAdded() {
  base::RecordAction(base::UserMetricsAction("Bookmarks.Added"));
}

void RecordBookmarkOpened() {
  base::RecordAction(base::UserMetricsAction("Bookmarks.Opened"));
}

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

void RecordUrlLoadStatsOnProfileLoad(const UrlLoadStats& stats) {
  DCHECK_LE(stats.duplicate_url_bookmark_count, stats.total_url_bookmark_count);
  DCHECK_LE(stats.duplicate_url_and_title_bookmark_count,
            stats.duplicate_url_bookmark_count);
  DCHECK_LE(stats.duplicate_url_and_title_and_parent_bookmark_count,
            stats.duplicate_url_and_title_bookmark_count);

  base::UmaHistogramCounts100000(
      "Bookmarks.Count.OnProfileLoad",
      base::saturated_cast<int>(stats.total_url_bookmark_count));

  if (stats.duplicate_url_bookmark_count != 0) {
    base::UmaHistogramCounts100000(
        "Bookmarks.Count.OnProfileLoad.DuplicateUrl2",
        base::saturated_cast<int>(stats.duplicate_url_bookmark_count));
  }

  if (stats.duplicate_url_and_title_bookmark_count != 0) {
    base::UmaHistogramCounts100000(
        "Bookmarks.Count.OnProfileLoad.DuplicateUrlAndTitle",
        base::saturated_cast<int>(
            stats.duplicate_url_and_title_bookmark_count));
  }

  if (stats.duplicate_url_and_title_and_parent_bookmark_count != 0) {
    base::UmaHistogramCounts100000(
        "Bookmarks.Count.OnProfileLoad.DuplicateUrlAndTitleAndParent",
        base::saturated_cast<int>(
            stats.duplicate_url_and_title_and_parent_bookmark_count));
  }

  // Log derived metrics for convenience.
  base::UmaHistogramCounts100000(
      "Bookmarks.Count.OnProfileLoad.UniqueUrl",
      base::saturated_cast<int>(stats.total_url_bookmark_count -
                                stats.duplicate_url_bookmark_count));
  base::UmaHistogramCounts100000(
      "Bookmarks.Count.OnProfileLoad.UniqueUrlAndTitle",
      base::saturated_cast<int>(stats.total_url_bookmark_count -
                                stats.duplicate_url_and_title_bookmark_count));
  base::UmaHistogramCounts100000(
      "Bookmarks.Count.OnProfileLoad.UniqueUrlAndTitleAndParent",
      base::saturated_cast<int>(
          stats.total_url_bookmark_count -
          stats.duplicate_url_and_title_and_parent_bookmark_count));
  base::UmaHistogramCounts1000(
      "Bookmarks.Times.OnProfileLoad.TimeSinceAdded",
      base::saturated_cast<int>(stats.avg_num_days_since_added));
}

}  // namespace bookmarks::metrics
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_metrics.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "components/bookmarks/common/url_load_stats.h"

namespace bookmarks::metrics {

namespace {

const int kBytesPerKB = 1024;

void RecordBookmarkParentFolderType(BookmarkFolderTypeForUMA parent) {
  base::UmaHistogramEnumeration("Bookmarks.ParentFolderType", parent);
}

std::string GetStorageStateSuffixForMetrics(StorageStateForUma storage_state) {
  switch (storage_state) {
    case StorageStateForUma::kAccount:
      return std::string(".AccountStorage");
    case StorageStateForUma::kLocalOnly:
      return std::string(".LocalStorage");
    case StorageStateForUma::kSyncEnabled:
      return std::string(".LocalStorageSyncing");
  }
  NOTREACHED();
}

std::string GetStorageFileSuffixForMetrics(StorageFileForUma storage_file) {
  switch (storage_file) {
    case StorageFileForUma::kLocalOrSyncable:
      return std::string(".LocalOrSyncable");
    case StorageFileForUma::kAccount:
      return std::string(".Account");
  }
  NOTREACHED();
}

}  // namespace

void RecordUrlBookmarkAdded(BookmarkFolderTypeForUMA parent,
                            StorageStateForUma storage_state) {
  base::RecordAction(base::UserMetricsAction("Bookmarks.Added"));
  base::RecordComputedAction(base::StrCat(
      {"Bookmarks.Added", GetStorageStateSuffixForMetrics(storage_state)}));
  RecordBookmarkParentFolderType(parent);
}

void RecordBookmarkFolderAdded(BookmarkFolderTypeForUMA parent,
                               StorageStateForUma storage_state) {
  base::RecordAction(base::UserMetricsAction("Bookmarks.FolderAdded"));
  base::RecordComputedAction(
      base::StrCat({"Bookmarks.FolderAdded",
                    GetStorageStateSuffixForMetrics(storage_state)}));
  RecordBookmarkParentFolderType(parent);
}

void RecordBookmarkRemoved(BookmarkEditSource source) {
  base::UmaHistogramEnumeration("Bookmarks.RemovedSource", source);
}

void RecordBookmarkOpened(base::Time now,
                          base::Time date_last_used,
                          base::Time date_added,
                          StorageStateForUma storage_state) {
  if (date_last_used != base::Time()) {
    base::UmaHistogramCounts10000("Bookmarks.Opened.TimeSinceLastUsed",
                                  (now - date_last_used).InDays());
  }
  base::UmaHistogramCounts10000("Bookmarks.Opened.TimeSinceAdded",
                                (now - date_added).InDays());

  base::RecordAction(base::UserMetricsAction("Bookmarks.Opened"));
  base::RecordComputedAction(base::StrCat(
      {"Bookmarks.Opened", GetStorageStateSuffixForMetrics(storage_state)}));
}

void RecordBookmarkMovedTo(BookmarkFolderTypeForUMA new_parent) {
  RecordBookmarkParentFolderType(new_parent);
}

void RecordTimeSinceLastScheduledSave(base::TimeDelta delta) {
  base::UmaHistogramLongTimes("Bookmarks.Storage.TimeSinceLastScheduledSave",
                              delta);
}

void RecordTimeToLoadAtStartup(base::TimeDelta delta) {
  UmaHistogramTimes("Bookmarks.Storage.TimeToLoadAtStartup2", delta);
}

void RecordFileSizeAtStartup(int64_t total_bytes) {
  int total_size_kb = base::saturated_cast<int>(total_bytes / kBytesPerKB);
  base::UmaHistogramCounts1M("Bookmarks.Storage.FileSizeAtStartup2",
                             total_size_kb);
}

void RecordURLEdit(BookmarkEditSource source) {
  base::UmaHistogramEnumeration("Bookmarks.EditURLSource", source);
}

void RecordTitleEdit(BookmarkEditSource source) {
  base::UmaHistogramEnumeration("Bookmarks.EditTitleSource", source);
}

void RecordUrlLoadStatsOnProfileLoad(const UrlLoadStats& stats) {
  base::UmaHistogramCounts100000(
      "Bookmarks.Count.OnProfileLoad3",
      base::saturated_cast<int>(stats.total_url_bookmark_count));

  base::UmaHistogramCounts1000(
      "Bookmarks.Times.OnProfileLoad.TimeSinceAdded3",
      base::saturated_cast<int>(stats.avg_num_days_since_added));

  int utilization;
  if (stats.used_url_bookmark_count == 0) {
    utilization = 0;
  } else {
    // Calculate the utilization as a percentage from 0 - 100. Do this without
    // a float conversion by multiplying everything by 100 first.
    utilization = (100 * stats.used_url_bookmark_count +
                   stats.total_url_bookmark_count / 2) /
                  stats.total_url_bookmark_count;
  }

  for (size_t num_days_since_used : stats.per_bookmark_num_days_since_used) {
    base::UmaHistogramCounts1000(
        "Bookmarks.UtilizationPerBookmark.OnProfileLoad.DaysSinceUsed",
        base::saturated_cast<int>(num_days_since_used));
  };

  base::UmaHistogramPercentage(
      "Bookmarks.Utilization.OnProfileLoad.PercentageUsed", utilization);
  base::UmaHistogramCounts1000("Bookmarks.Utilization.OnProfileLoad.TotalUsed",
                               stats.used_url_bookmark_count);
  base::UmaHistogramCounts1000(
      "Bookmarks.Utilization.OnProfileLoad.TotalUnused",
      stats.total_url_bookmark_count - stats.used_url_bookmark_count);

  if (stats.most_recently_used_bookmark_days != SIZE_MAX) {
    base::UmaHistogramCounts1000(
        "Bookmarks.Times.OnProfileLoad.MostRecentlyUsedBookmarkInDays",
        base::saturated_cast<int>(stats.most_recently_used_bookmark_days));
  }

  if (stats.most_recently_saved_bookmark_days != SIZE_MAX) {
    base::UmaHistogramCounts1000(
        "Bookmarks.Times.OnProfileLoad.MostRecentlySavedBookmarkInDays",
        base::saturated_cast<int>(stats.most_recently_saved_bookmark_days));
  }

  if (stats.most_recently_saved_folder_days != SIZE_MAX) {
    base::UmaHistogramCounts1000(
        "Bookmarks.Times.OnProfileLoad.MostRecentlyAddedFolderInDays",
        base::saturated_cast<int>(stats.most_recently_saved_folder_days));
  }
}

void RecordCloneBookmarkNode(int num_cloned) {
  base::UmaHistogramCounts100("Bookmarks.Clone.NumCloned", num_cloned);
}

void RecordAverageNodeSizeAtStartup(size_t size_in_bytes) {
  base::UmaHistogramCounts10000("Bookmarks.AverageNodeSize", size_in_bytes);
}

void RecordIdsReassignedOnProfileLoad(StorageFileForUma storage_file,
                                      bool ids_reassigned) {
  base::UmaHistogramBoolean(
      base::StrCat({"Bookmarks.IdsReassigned.OnProfileLoad",
                    GetStorageFileSuffixForMetrics(storage_file)}),
      ids_reassigned);
}

}  // namespace bookmarks::metrics

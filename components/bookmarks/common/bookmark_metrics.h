// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_METRICS_H_
#define COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_METRICS_H_

#include "base/time/time.h"

namespace bookmarks {

struct UrlLoadStats;

namespace metrics {

// Enum for folder categories, reported through UMA. Present in enums.xml as
// BookmarkFolderType. New values should be added at the end and things should
// not be renumbered.
enum class BookmarkFolderTypeForUMA {
  kBookmarksBar = 0,
  kOtherBookmarks = 1,
  kMobileBookmarks = 2,
  kUserGeneratedFolder = 3,
  kMaxValue = kUserGeneratedFolder,
};

// Enum for possible sources for edits, reported through UMA. Present in
// enums.xml as BookmarkEditSource. New values should be added at the end
// and things should not be renumbered.
enum class BookmarkEditSource {
  kUser = 0,
  kExtension = 1,
  // No need to add a type for every possible scenario, we mainly care about if
  // the user did the edit.
  kOther = 2,
  kMaxValue = kOther,
};

// An enum class to add storage state as a suffix to metrics.
enum class StorageStateForUma {
  // Account storage.
  kAccount,
  // Local storage that is not being synced at the time the metric is
  // recorded.
  kLocalOnly,
  // Local storage that is being synced at the time the metric is recorded.
  kSyncEnabled,
};

// An enum class representing the two JSON files for storing bookmarks, used for
// suffixing metrics.
enum class StorageFileForUma {
  // Represents `kLocalOrSyncableBookmarksFileName`.
  kLocalOrSyncable,
  // Represents `kAccountBookmarksFileName`.
  kAccount,
};

// Records when a bookmark is added by the user.
void RecordUrlBookmarkAdded(BookmarkFolderTypeForUMA parent,
                            StorageStateForUma storage_state);

// Records when a bookmark folder is added by the user.
void RecordBookmarkFolderAdded(BookmarkFolderTypeForUMA parent,
                               StorageStateForUma storage_state);

// Records when a bookmark is removed.
void RecordBookmarkRemoved(BookmarkEditSource source);

// Records when a bookmark is opened by the user.
void RecordBookmarkOpened(base::Time now,
                          base::Time date_last_used,
                          base::Time date_added,
                          StorageStateForUma storage_state);

// Records when a bookmark or bookmark folder is moved to a different parent
// folder.
void RecordBookmarkMovedTo(BookmarkFolderTypeForUMA new_parent);

// Records the time since the last save with a 1 hour max. The first save will
// record the time since startup.
void RecordTimeSinceLastScheduledSave(base::TimeDelta delta);

// Records the time it takes to load the bookmark model on startup with a 10
// second max, the time starts when BookmarkModel.Load is called.
void RecordTimeToLoadAtStartup(base::TimeDelta delta);

// Records size of the bookmark file at startup.
void RecordFileSizeAtStartup(int64_t total_bytes);

// Records a bookmark URL edit.
void RecordURLEdit(BookmarkEditSource source);

// Records a bookmark URL edit.
void RecordTitleEdit(BookmarkEditSource source);

// Records the metrics derived from `stats`. Recording happens on profile load.
void RecordUrlLoadStatsOnProfileLoad(const UrlLoadStats& stats);

// Records when a bookmark node is cloned. `num_cloned` is the number of
// bookmarks that were selected.
void RecordCloneBookmarkNode(int num_cloned);

// Records the approximate average node size at startup.
void RecordAverageNodeSizeAtStartup(size_t size_in_bytes);

// Records whether or not node IDs were reassigned as a result of loading the
// JSON file representing local-or-syncable bookmarks.
void RecordIdsReassignedOnProfileLoad(StorageFileForUma storage_file,
                                      bool ids_reassigned);

}  // namespace metrics

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_METRICS_H_

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_METRICS_H_
#define COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_METRICS_H_

#include "base/time/time.h"

namespace bookmarks {

struct UrlLoadStats;

namespace metrics {

// Records when a bookmark is added by the user.
void RecordBookmarkAdded();

// Records when a bookmark is opened by the user.
void RecordBookmarkOpened(base::Time now,
                          base::Time date_last_used,
                          base::Time date_added);

// Records the time since the last save with a 1 hour max. The first save will
// record the time since startup.
void RecordTimeSinceLastScheduledSave(base::TimeDelta delta);

// Records the time it takes to load the bookmark model on startup with a 10
// second max, the time starts when BookmarkModel.Load is called.
void RecordTimeToLoadAtStartup(base::TimeDelta delta);

// Records size of the bookmark file at startup.
void RecordFileSizeAtStartup(int64_t total_bytes);

// Records the metrics derived from `stats`. Recording happens on profile load.
void RecordUrlLoadStatsOnProfileLoad(const UrlLoadStats& stats);

// Records when a bookmark node is cloned. `num_cloned` is the number of
// bookmarks that were selected.
void RecordCloneBookmarkNode(int num_cloned);

}  // namespace metrics

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_METRICS_H_
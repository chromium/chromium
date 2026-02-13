// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_CONSTANTS_H_
#define COMPONENTS_SYNC_BOOKMARKS_CONSTANTS_H_

#include <stddef.h>

namespace sync_bookmarks {

// The maximum number of bookmarks that are allowed to be synced. If the number
// of local bookmarks exceeds this limit, sync will enter a "bookmarks limit
// exceeded" error state.
// LINT.IfChange(SyncBookmarksLimit)
inline constexpr size_t kSyncBookmarksLimit = 100000;
// LINT.ThenChange(//components/sync/android/java/src/org/chromium/components/sync/SyncService.java:SyncBookmarksLimit)

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_CONSTANTS_H_

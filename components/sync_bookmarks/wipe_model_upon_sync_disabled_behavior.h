// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_WIPE_MODEL_UPON_SYNC_DISABLED_BEHAVIOR_H_
#define COMPONENTS_SYNC_BOOKMARKS_WIPE_MODEL_UPON_SYNC_DISABLED_BEHAVIOR_H_

namespace sync_bookmarks {

// Determines whether and under which circumstances the sync code dealing with a
// specific BookmarkModel instance should invoke RemoveAllUserBookmarks().
enum class WipeModelUponSyncDisabledBehavior {
  // BookmarkModel::RemoveAllUserBookmarks() is never invoked. In this case, the
  // lifetime of local bookmarks is fully decoupled from sync metadata's.
  kNever,
  // BookmarkModel::RemoveAllUserBookmarks() is invoked every time sync is
  // permanently disabled (useful for account storage). In this case, the
  // lifetime of local bookmarks is fully coupled with sync metadata's.
  kAlways,
  // BookmarkModel::RemoveAllUserBookmarks() is invoked at most once, the next
  // time sync is permanently disabled. In practice, this is currently used on
  // iOS only, in advanced cases involving restored-from-backup sync bookmarks.
  kOnceIfTrackingMetadata,
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_WIPE_MODEL_UPON_SYNC_DISABLED_BEHAVIOR_H_

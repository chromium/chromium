// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_STORAGE_TYPE_H_
#define COMPONENTS_BOOKMARKS_COMMON_STORAGE_TYPE_H_

namespace bookmarks {

// Defines the data storage strategy used by a BookmarkModel.
// Do not change the explicitly set values. This enum is saved in preference
// kIosBookmarkLastUsedStorageReceivingBookmarks.
enum class StorageType {
  // Local storage indicates that the data in the BookmarkModel can't be
  // explicitly attributed to an account and the behavior depends on the state
  // of sync-the-feature. If sync-the-feature is off - the model data is
  // local. If sync-the-feature is on - the model data is synced.
  kLocalOrSyncable = 0,
  // Account storage indicates all data can be attributed to an account, which
  // also means the data will be removed from the BookmarkModel when the user
  // signs out.
  kAccount = 1,
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_STORAGE_TYPE_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_TO_ACCOUNT_MERGER_H_
#define COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_TO_ACCOUNT_MERGER_H_

#include "base/memory/raw_ptr.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace sync_bookmarks {

// Class responsible for implementing the merge algorithm that allows moving all
// local bookmarks to become account bookmarks, with the ability to dedup data.
class LocalBookmarkToAccountMerger {
 public:
  // `model` must not be null and must outlive this object. It must also be
  // loaded and with existing permanent folders for account bookmarks.
  explicit LocalBookmarkToAccountMerger(bookmarks::BookmarkModel* model);

  LocalBookmarkToAccountMerger(const LocalBookmarkToAccountMerger&) = delete;
  LocalBookmarkToAccountMerger& operator=(const LocalBookmarkToAccountMerger&) =
      delete;

  ~LocalBookmarkToAccountMerger();

  // Merges local bookmarks into account bookmarks.
  void MoveAndMerge();

 private:
  const raw_ptr<bookmarks::BookmarkModel> model_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_TO_ACCOUNT_MERGER_H_

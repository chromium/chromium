// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/local_bookmark_to_account_merger.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/local_bookmark_model_merger.h"

namespace sync_bookmarks {

LocalBookmarkToAccountMerger::LocalBookmarkToAccountMerger(
    bookmarks::BookmarkModel* model)
    : model_(model) {
  CHECK(model_);
  CHECK(model_->loaded());
  CHECK(model_->account_bookmark_bar_node());
  CHECK(model_->account_other_node());
  CHECK(model_->account_mobile_node());
}

LocalBookmarkToAccountMerger::~LocalBookmarkToAccountMerger() = default;

void LocalBookmarkToAccountMerger::MoveAndMerge() {
  BookmarkModelViewUsingLocalOrSyncableNodes
      local_or_syncable_bookmark_model_view(model_);
  BookmarkModelViewUsingAccountNodes account_bookmark_model_view(model_);

  // TODO(crbug.com/332532186): Implement move-and-merge logic without depending
  // on LocalBookmarkModelMerger and definitely without exercising
  // RemoveAllSyncableNodes().
  LocalBookmarkModelMerger(&local_or_syncable_bookmark_model_view,
                           &account_bookmark_model_view)
      .Merge();
  local_or_syncable_bookmark_model_view.RemoveAllSyncableNodes();
}

}  // namespace sync_bookmarks

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/local_bookmark_model_merger.h"
#include "components/sync_bookmarks/local_bookmark_to_account_merger.h"
#include "components/sync_bookmarks/switches.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

BookmarkLocalDataBatchUploader::BookmarkLocalDataBatchUploader(
    bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {}

BookmarkLocalDataBatchUploader::~BookmarkLocalDataBatchUploader() = default;

void BookmarkLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  if (!CanUpload()) {
    std::move(callback).Run(syncer::LocalDataDescription());
    return;
  }

  BookmarkModelViewUsingLocalOrSyncableNodes
      local_or_syncable_bookmark_model_view(bookmark_model_);

  std::vector<GURL> bookmarked_urls;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      local_or_syncable_bookmark_model_view.root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    // Skip folders and non-syncable nodes (e.g. managed bookmarks).
    if (node->is_url() &&
        local_or_syncable_bookmark_model_view.IsNodeSyncable(node)) {
      bookmarked_urls.push_back(node->url());
    }
  }
  std::move(callback).Run(syncer::LocalDataDescription(bookmarked_urls));
}

void BookmarkLocalDataBatchUploader::TriggerLocalDataMigration() {
  if (!CanUpload()) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          switches::kSyncMinimizeDeletionsDuringBookmarkBatchUpload)) {
    LocalBookmarkToAccountMerger(bookmark_model_).MoveAndMerge();
  } else {
    BookmarkModelViewUsingLocalOrSyncableNodes
        local_or_syncable_bookmark_model_view(bookmark_model_);
    BookmarkModelViewUsingAccountNodes account_bookmark_model_view(
        bookmark_model_);

    LocalBookmarkModelMerger(&local_or_syncable_bookmark_model_view,
                             &account_bookmark_model_view)
        .Merge();
    local_or_syncable_bookmark_model_view.RemoveAllSyncableNodes();
  }
}

bool BookmarkLocalDataBatchUploader::CanUpload() const {
  return bookmark_model_ && bookmark_model_->loaded() &&
         bookmark_model_->account_bookmark_bar_node();
}

}  // namespace sync_bookmarks

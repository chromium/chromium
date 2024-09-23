// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/features.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/local_bookmark_model_merger.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

BookmarkLocalDataBatchUploader::BookmarkLocalDataBatchUploader(
    bookmarks::BookmarkModel* bookmark_model)
    : local_or_syncable_bookmark_model_view_(
          bookmark_model
              ? std::make_unique<BookmarkModelViewUsingLocalOrSyncableNodes>(
                    bookmark_model)
              : nullptr),
      account_bookmark_model_view_(
          bookmark_model && base::FeatureList::IsEnabled(
                                syncer::kSyncEnableBookmarksInTransportMode)
              ? std::make_unique<BookmarkModelViewUsingAccountNodes>(
                    bookmark_model)
              : nullptr) {}

BookmarkLocalDataBatchUploader::~BookmarkLocalDataBatchUploader() = default;

void BookmarkLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  if (!CanUpload()) {
    std::move(callback).Run(syncer::LocalDataDescription());
    return;
  }

  std::vector<GURL> bookmarked_urls;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      local_or_syncable_bookmark_model_view_->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    // Skip folders and non-syncable nodes (e.g. managed bookmarks).
    if (node->is_url() &&
        local_or_syncable_bookmark_model_view_->IsNodeSyncable(node)) {
      bookmarked_urls.push_back(node->url());
    }
  }
  std::move(callback).Run(syncer::LocalDataDescription(bookmarked_urls));
}

void BookmarkLocalDataBatchUploader::TriggerLocalDataMigration() {
  if (!CanUpload()) {
    return;
  }

  // Guard against absence of account bookmarks. For example, this can
  // happen if the initial download hasn't completed.
  // TODO(crbug.com/354146311): Move to CanUpload().
  if (account_bookmark_model_view_->bookmark_bar_node()) {
    sync_bookmarks::LocalBookmarkModelMerger(
        local_or_syncable_bookmark_model_view_.get(),
        account_bookmark_model_view_.get())
        .Merge();
    local_or_syncable_bookmark_model_view_->RemoveAllSyncableNodes();
  }
}

bool BookmarkLocalDataBatchUploader::CanUpload() const {
  return local_or_syncable_bookmark_model_view_ &&
         account_bookmark_model_view_ &&
         local_or_syncable_bookmark_model_view_->loaded() &&
         account_bookmark_model_view_->loaded();
}

}  // namespace sync_bookmarks

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/local_bookmark_model_merger.h"
#include "components/sync_bookmarks/local_bookmark_to_account_merger.h"
#include "components/sync_bookmarks/switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

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

  // TODO(crbug.com/380818406): migrate away from
  // BookmarkModelViewUsingLocalOrSyncableNodes and use BookmarkModel directly.
  BookmarkModelViewUsingLocalOrSyncableNodes
      local_or_syncable_bookmark_model_view(bookmark_model_);

  std::vector<syncer::LocalDataItemModel> local_data_items;
  std::vector<GURL> bookmarked_urls;

  for (const auto& permanent_node :
       local_or_syncable_bookmark_model_view.root_node()->children()) {
    for (const auto& node : permanent_node->children()) {
      // Generate a bookmark item for each top-level folder or bookmark in the
      // tree.
      if (!local_or_syncable_bookmark_model_view.IsNodeSyncable(node.get())) {
        // Skip non-syncable nodes (e.g. managed bookmarks).
        continue;
      }
      std::vector<GURL> urls = GetBookmarkedUrlsInSubtree(
          local_or_syncable_bookmark_model_view, node.get());
      bookmarked_urls.insert(bookmarked_urls.end(), urls.begin(), urls.end());

      if (base::FeatureList::IsEnabled(
              switches::kSyncBookmarksBatchUploadSelectedItems)) {
        // Populate the individual items for Batch Upload (used on
        // Windows/Mac/Linux) only.
        local_data_items.push_back(
            DataItemModelFromNode(node.get(), urls.size()));
      }
    }
  }

  auto local_data_description = syncer::LocalDataDescription(bookmarked_urls);
  local_data_description.type = syncer::DataType::BOOKMARKS;
  local_data_description.local_data_models = std::move(local_data_items);
  std::move(callback).Run(local_data_description);
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

std::vector<GURL> BookmarkLocalDataBatchUploader::GetBookmarkedUrlsInSubtree(
    const BookmarkModelViewUsingLocalOrSyncableNodes&
        local_or_syncable_bookmark_model_view,
    const bookmarks::BookmarkNode* subtree_root) const {
  CHECK(subtree_root);

  std::vector<GURL> bookmarked_urls;

  if (subtree_root->is_url()) {
    bookmarked_urls.push_back(subtree_root->url());
  }

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(subtree_root);
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    if (!local_or_syncable_bookmark_model_view.IsNodeSyncable(node)) {
      // Skip non-syncable nodes (e.g. managed bookmarks).
      continue;
    }

    // Build up the list of bookmarked URLs, used for the dialog on mobile
    // platforms.
    if (node->is_url()) {
      bookmarked_urls.push_back(node->url());
    }
  }
  return bookmarked_urls;
}

syncer::LocalDataItemModel
BookmarkLocalDataBatchUploader::DataItemModelFromNode(
    const bookmarks::BookmarkNode* node,
    int bookmarked_urls_count) const {
  CHECK(node);
  CHECK(!node->is_permanent_node());

  syncer::LocalDataItemModel item;

  item.id = node->id();
  if (node->is_folder()) {
    // TODO(crbug.com/380818406): set the static folder icon in item.icon_url.
    item.title = base::UTF16ToUTF8(node->GetTitledUrlNodeTitle());
    item.subtitle = l10n_util::GetPluralStringFUTF8(
        IDS_BULK_UPLOAD_BOOKMARK_FOLDER_SUBTITLE, bookmarked_urls_count);
  } else {
    CHECK(node->is_url());
    CHECK_EQ(bookmarked_urls_count, 1);

    // TODO(crbug.com/380818406): fallback to the default icon.
    if (node->icon_url()) {
      item.icon_url = *node->icon_url();
    }
    item.title = base::UTF16ToUTF8(node->GetTitle());
  }

  return item;
}

}  // namespace sync_bookmarks

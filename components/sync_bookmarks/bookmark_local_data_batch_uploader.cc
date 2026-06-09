// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <utility>
#include <variant>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/local_bookmark_to_account_merger.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

namespace sync_bookmarks {

namespace {

// Returns true if there is any local (syncable) bookmark data available under
// the main local permanent folders.
bool HasLocalDataToUpload(const bookmarks::BookmarkModel* model) {
  CHECK(model->bookmark_bar_node());
  CHECK(model->other_node());
  CHECK(model->mobile_node());
  return !model->bookmark_bar_node()->children().empty() ||
         !model->other_node()->children().empty() ||
         !model->mobile_node()->children().empty();
}

void RecordBookmarksDisabledDueToLimitExceeded(bool limit_exceeded) {
  base::UmaHistogramBoolean(
      "Sync.BatchUpload.BookmarksDisabledDueToLimitExceeded", limit_exceeded);
}

}  // namespace

BookmarkLocalDataBatchUploader::BookmarkLocalDataBatchUploader(
    bookmarks::BookmarkModel* bookmark_model,
    PrefService* pref_service)
    : bookmark_model_(bookmark_model), pref_service_(pref_service) {}

BookmarkLocalDataBatchUploader::~BookmarkLocalDataBatchUploader() = default;

void BookmarkLocalDataBatchUploader::SetMaxBookmarksLimitForTesting(
    size_t limit) {
  max_bookmarks_limit_ = limit;
}

void BookmarkLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  switch (DetermineAbilityToUpload()) {
    case CanUploadResult::kNotAllowed:
      // No upload is possible, and the metric is not logged (either because
      // prerequisites are missing, or there is no local data to upload).
      std::move(callback).Run(syncer::LocalDataDescription());
      return;
    case CanUploadResult::kLimitExceeded:
      // Local data exists but upload is disabled due to the limit. Log this
      // event.
      RecordBookmarksDisabledDueToLimitExceeded(true);
      std::move(callback).Run(syncer::LocalDataDescription());
      return;
    case CanUploadResult::kAllowed:
      // Upload is possible. Log that limit is not exceeded and proceed to
      // build the description.
      RecordBookmarksDisabledDueToLimitExceeded(false);
      break;
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

      // Populate the individual items for Batch Upload (used on
      // Windows/Mac/Linux) only.
      local_data_items.push_back(
          DataItemModelFromNode(node.get(), urls.size()));
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

  LocalBookmarkToAccountMerger(bookmark_model_).MoveAndMergeAllNodes();
}

void BookmarkLocalDataBatchUploader::TriggerLocalDataMigrationForItems(
    std::vector<syncer::LocalDataItemModel::DataId> items) {
  if (!CanUpload()) {
    return;
  }

  std::set<int64_t> ids;
  std::transform(items.begin(), items.end(), std::inserter(ids, ids.begin()),
                 [](const syncer::LocalDataItemModel::DataId& id) {
                   return std::get<int64_t>(id);
                 });

  LocalBookmarkToAccountMerger(bookmark_model_)
      .MoveAndMergeSpecificSubtrees(std::move(ids));
}

BookmarkLocalDataBatchUploader::CanUploadResult
BookmarkLocalDataBatchUploader::DetermineAbilityToUpload() const {
  if (!bookmark_model_ || !bookmark_model_->loaded() ||
      !bookmark_model_->account_bookmark_bar_node() ||
      !pref_service_->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled)) {
    return CanUploadResult::kNotAllowed;
  }

  if (!HasLocalDataToUpload(bookmark_model_)) {
    return CanUploadResult::kNotAllowed;
  }

  // Note: This is a conservative check as it includes permanent folders,
  // root node, and managed bookmarks. It also does not account for potential
  // deduplication during the merge.
  if (bookmark_model_->GetTotalNumberOfUrlsAndFoldersIncludingManagedNodes() >
      max_bookmarks_limit_) {
    return CanUploadResult::kLimitExceeded;
  }

  return CanUploadResult::kAllowed;
}

bool BookmarkLocalDataBatchUploader::CanUpload() const {
  switch (DetermineAbilityToUpload()) {
    case CanUploadResult::kAllowed:
      return true;
    case CanUploadResult::kNotAllowed:
    case CanUploadResult::kLimitExceeded:
      return false;
  }
  NOTREACHED();
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
    item.icon = syncer::LocalDataItemModel::FolderIcon();
    item.title = base::UTF16ToUTF8(node->GetTitledUrlNodeTitle());
    item.subtitle = l10n_util::GetPluralStringFUTF8(
        IDS_BULK_UPLOAD_BOOKMARK_FOLDER_SUBTITLE, bookmarked_urls_count);
  } else {
    CHECK(node->is_url());
    CHECK_EQ(bookmarked_urls_count, 1);
    item.icon = syncer::LocalDataItemModel::PageUrlIcon(node->url());
    item.title = base::UTF16ToUTF8(node->GetTitle());
  }

  return item;
}

}  // namespace sync_bookmarks

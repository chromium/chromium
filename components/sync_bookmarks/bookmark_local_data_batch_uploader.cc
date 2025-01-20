// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <utility>
#include <variant>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
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
namespace {

// Returns the number of descendants (folders or URLs) under `parent` (excluding
// `parent` itself).
size_t GetNumberOfDescendants(const bookmarks::BookmarkNode* parent) {
  if (!parent) {
    return 0;
  }

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(parent);
  size_t num_nodes = 0;
  while (iterator.has_next()) {
    num_nodes++;
    iterator.Next();
  }
  return num_nodes;
}

// Returns the number of local nodes (folders or URLs) in `bookmark_model`,
// excluding managed nodes.
size_t GetNumberOfLocalNodes(const bookmarks::BookmarkModel* bookmark_model) {
  CHECK(bookmark_model);
  return GetNumberOfDescendants(bookmark_model->mobile_node()) +
         GetNumberOfDescendants(bookmark_model->bookmark_bar_node()) +
         GetNumberOfDescendants(bookmark_model->other_node());
}

// Returns the number of account nodes (folders or URLs) in `bookmark_model`.
size_t GetNumberOfAccountNodes(const bookmarks::BookmarkModel* bookmark_model) {
  CHECK(bookmark_model);
  return GetNumberOfDescendants(bookmark_model->account_mobile_node()) +
         GetNumberOfDescendants(bookmark_model->account_bookmark_bar_node()) +
         GetNumberOfDescendants(bookmark_model->account_other_node());
}

}  // namespace

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
              switches::kSyncBookmarksBatchUploadSelectedItems) &&
          base::FeatureList::IsEnabled(
              switches::kSyncMinimizeDeletionsDuringBookmarkBatchUpload)) {
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

  const size_t num_local_nodes_before = GetNumberOfLocalNodes(bookmark_model_);
  const size_t num_account_nodes_before =
      GetNumberOfAccountNodes(bookmark_model_);
  const size_t num_total_nodes_before =
      num_local_nodes_before + num_account_nodes_before;

  {
    base::ScopedUmaHistogramTimer scoped_timer(
        kBatchUploadDurationHistogramName,
        base::ScopedUmaHistogramTimer::ScopedHistogramTiming::kMediumTimes);

    if (base::FeatureList::IsEnabled(
            switches::kSyncMinimizeDeletionsDuringBookmarkBatchUpload)) {
      LocalBookmarkToAccountMerger(bookmark_model_).MoveAndMergeAllNodes();
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

  // All local nodes should have been merged into account nodes.
  CHECK_EQ(GetNumberOfLocalNodes(bookmark_model_), 0u);
  const size_t num_account_nodes_after =
      GetNumberOfAccountNodes(bookmark_model_);

  // Batch upload should not create additional nodes.
  CHECK_LE(num_account_nodes_after, num_total_nodes_before);

  // Batch upload should, at most, deduplicate all local nodes. The number of
  // of resulting account nodes cannot be smaller than the original number of
  // account nodes.
  CHECK_GE(num_account_nodes_after, num_account_nodes_before);
  // Similarly, the number of resulting account nodes cannot be smaller than the
  // original number of local nodes (as deduplication logic never deduplicates
  // more than two nodes into one).
  CHECK_GE(num_account_nodes_after, num_local_nodes_before);
  // As a corollary, the total number of nodes can at most reduce by 50% if all
  // local nodes are deduplicated.
  CHECK_GE(num_account_nodes_after * 2, num_total_nodes_before);

  base::UmaHistogramCounts100000("Bookmarks.BatchUploadOutcomeAccountNodes",
                                 num_account_nodes_after);

  if (num_total_nodes_before != 0) {
    const double ratio = 1.0 * num_account_nodes_after / num_total_nodes_before;
    base::UmaHistogramPercentage("Bookmarks.BatchUploadOutcomeRatio",
                                 static_cast<int>(std::round(100.0 * ratio)));
  }
}

void BookmarkLocalDataBatchUploader::TriggerLocalDataMigrationForItems(
    std::vector<syncer::LocalDataItemModel::DataId> items) {
  // The per-item batch upload UI requires this new code path. The entry point
  // is hidden if this feature flag is disabled so it's safe to CHECK here.
  CHECK(base::FeatureList::IsEnabled(
      switches::kSyncMinimizeDeletionsDuringBookmarkBatchUpload));

  if (!CanUpload()) {
    return;
  }

  const size_t num_local_nodes_before = GetNumberOfLocalNodes(bookmark_model_);
  const size_t num_account_nodes_before =
      GetNumberOfAccountNodes(bookmark_model_);
  const size_t num_total_nodes_before =
      num_local_nodes_before + num_account_nodes_before;

  {
    base::ScopedUmaHistogramTimer scoped_timer(
        kBatchUploadDurationHistogramName,
        base::ScopedUmaHistogramTimer::ScopedHistogramTiming::kMediumTimes);

    std::set<int64_t> ids;
    std::transform(items.begin(), items.end(), std::inserter(ids, ids.begin()),
                   [](const syncer::LocalDataItemModel::DataId& id) {
                     return std::get<int64_t>(id);
                   });
    LocalBookmarkToAccountMerger(bookmark_model_)
        .MoveAndMergeSpecificSubtrees(std::move(ids));
  }

  const size_t num_local_nodes_after = GetNumberOfLocalNodes(bookmark_model_);
  const size_t num_account_nodes_after =
      GetNumberOfAccountNodes(bookmark_model_);
  const size_t num_total_nodes_after =
      num_local_nodes_after + num_account_nodes_after;

  // Batch upload should not create additional nodes.
  CHECK_LE(num_total_nodes_after, num_total_nodes_before);

  // Batch upload should, at most, deduplicate all local nodes. The number of
  // of resulting account nodes cannot be smaller than the original number of
  // account nodes.
  CHECK_GE(num_account_nodes_after, num_account_nodes_before);
  // As a corollary, the total number of nodes can at most reduce by 50% if all
  // local nodes are deduplicated.
  CHECK_GE(num_total_nodes_after * 2, num_total_nodes_before);
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

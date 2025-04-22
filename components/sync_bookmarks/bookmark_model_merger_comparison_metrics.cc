// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger_comparison_metrics.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync_bookmarks/bookmark_model_merger.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

namespace sync_bookmarks::metrics {
namespace {

// Constants forked from bookmark_model_merger.cc.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

using RemoteForest = BookmarkModelMerger::RemoteForest;
using RemoteTreeNode = BookmarkModelMerger::RemoteTreeNode;

// Returns the subset of top-level permanent bookmark folders in
// `all_local_data` as selected by `subtree_selection`.
std::vector<const bookmarks::BookmarkNode*> GetRelevantLocalSubtrees(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection) {
  switch (subtree_selection) {
    case SubtreeSelection::kConsideringAllBookmarks:
      return {all_local_data.bookmark_bar_node(), all_local_data.other_node(),
              all_local_data.mobile_node()};
    case SubtreeSelection::kUnderBookmarkBar:
      return {all_local_data.bookmark_bar_node()};
  }
}

// Returns the subset of top-level permanent bookmark folder updates in
// `all_account_data` as selected by `subtree_selection`. Note that these
// updates also contain updates for their descendants.
std::vector<const RemoteTreeNode*> GetRelevantAccountSubtrees(
    const RemoteForest& all_account_data,
    SubtreeSelection subtree_selection) {
  std::vector<std::string> relevant_tags;
  switch (subtree_selection) {
    case SubtreeSelection::kConsideringAllBookmarks:
      relevant_tags.push_back(kMobileBookmarksTag);
      relevant_tags.push_back(kOtherBookmarksTag);
      [[fallthrough]];
    case SubtreeSelection::kUnderBookmarkBar:
      relevant_tags.push_back(kBookmarkBarTag);
      break;
  }
  std::vector<const RemoteTreeNode*> result;
  for (const std::string& tag : relevant_tags) {
    auto it = all_account_data.find(tag);
    if (it != all_account_data.end()) {
      result.push_back(&it->second);
    }
  }
  return result;
}

// Function template declaration that must be specialized per supported grouping
// criterion that allows mapping local data to keys used for grouping.
template <typename Key>
Key GroupingKeyFromLocalData(const bookmarks::BookmarkNode* node);

// Same as above but for account data.
template <typename Key>
Key GroupingKeyFromAccountData(const sync_pb::BookmarkSpecifics& specifics);

template <>
UrlAndTitle GroupingKeyFromLocalData(const bookmarks::BookmarkNode* node) {
  UrlAndTitle key;
  key.url = node->url();
  key.title = node->GetTitle();
  return key;
}

template <>
UrlAndTitle GroupingKeyFromAccountData(
    const sync_pb::BookmarkSpecifics& specifics) {
  UrlAndTitle key;
  key.url = GURL(specifics.url());
  key.title = NodeTitleFromSpecifics(specifics);
  return key;
}

template <>
UrlAndUuid GroupingKeyFromLocalData(const bookmarks::BookmarkNode* node) {
  UrlAndUuid key;
  key.url = node->url();
  key.uuid = node->uuid();
  return key;
}

template <>
UrlAndUuid GroupingKeyFromAccountData(
    const sync_pb::BookmarkSpecifics& specifics) {
  UrlAndUuid key;
  key.url = GURL(specifics.url());
  key.uuid = base::Uuid::ParseLowercase(specifics.guid());
  return key;
}

// Recursive function used to implement `ExtractLocalDataSet()` below.
template <typename Key>
void ExtractLocalDataSetRecursive(const bookmarks::BookmarkNode* node,
                                  std::vector<Key>& keys) {
  CHECK(node);
  if (node->is_url()) {
    keys.emplace_back(GroupingKeyFromLocalData<Key>(node));
    return;
  }
  for (const std::unique_ptr<bookmarks::BookmarkNode>& child :
       node->children()) {
    ExtractLocalDataSetRecursive(child.get(), keys);
  }
}

// Returns unique data items representing URL bookmarks in
// `relevant_local_subtrees` after being grouped by `Key`.
template <typename Key>
base::flat_set<Key> ExtractLocalDataSet(
    const std::vector<const bookmarks::BookmarkNode*>&
        relevant_local_subtrees) {
  std::vector<Key> keys;
  for (const bookmarks::BookmarkNode* node : relevant_local_subtrees) {
    ExtractLocalDataSetRecursive(node, keys);
  }
  return base::flat_set<Key>(std::move(keys));
}

// Recursive function used to implement `ExtractAccountDataSet()` below.
template <typename Key>
void ExtractAccountDataSetRecursive(const RemoteTreeNode& node,
                                    std::vector<Key>& keys) {
  if (node.entity().specifics.bookmark().type() ==
      sync_pb::BookmarkSpecifics::URL) {
    keys.emplace_back(
        GroupingKeyFromAccountData<Key>(node.entity().specifics.bookmark()));
    return;
  }
  for (const RemoteTreeNode& child : node.children()) {
    ExtractAccountDataSetRecursive(child, keys);
  }
}

// Returns unique data items representing URL bookmarks in
// `relevant_account_subtrees` after being grouped by `Key`.
template <typename Key>
base::flat_set<Key> ExtractAccountDataSet(
    const std::vector<const RemoteTreeNode*>& relevant_account_subtrees) {
  std::vector<Key> keys;
  for (const RemoteTreeNode* node : relevant_account_subtrees) {
    CHECK(node);
    ExtractAccountDataSetRecursive(*node, keys);
  }
  return base::flat_set<Key>(std::move(keys));
}

}  // namespace

base::flat_set<UrlAndTitle> ExtractUniqueLocalNodesByUrlAndTitleForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection) {
  return ExtractLocalDataSet<UrlAndTitle>(
      GetRelevantLocalSubtrees(all_local_data, subtree_selection));
}

base::flat_set<UrlAndUuid> ExtractUniqueLocalNodesByUrlAndUuidForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection) {
  return ExtractLocalDataSet<UrlAndUuid>(
      GetRelevantLocalSubtrees(all_local_data, subtree_selection));
}

base::flat_set<UrlAndTitle> ExtractUniqueAccountNodesByUrlAndTitleForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection) {
  return ExtractAccountDataSet<UrlAndTitle>(
      GetRelevantAccountSubtrees(all_account_data, subtree_selection));
}

base::flat_set<UrlAndUuid> ExtractUniqueAccountNodesByUrlAndUuidForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection) {
  return ExtractAccountDataSet<UrlAndUuid>(
      GetRelevantAccountSubtrees(all_account_data, subtree_selection));
}

}  // namespace sync_bookmarks::metrics

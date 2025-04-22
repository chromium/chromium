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
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
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

// Constants used when the parent's title is relevant during grouping. They
// resemble the real titles but the precise value isn't important, as long as
// they are unlikely to collide with user-generated folders.
constexpr char16_t kBookmarkBarFolderName[] = u"__Bookmarks bar__";
constexpr char16_t kOtherBookmarksFolderName[] = u"__Other bookmarks__";
constexpr char16_t kMobileBookmarksFolderName[] = u"__Mobile bookmarks__";

using RemoteForest = BookmarkModelMerger::RemoteForest;
using RemoteTreeNode = BookmarkModelMerger::RemoteTreeNode;

std::u16string_view GetBookmarkNodeTitle(const bookmarks::BookmarkNode* node) {
  CHECK(node);
  switch (node->type()) {
    case bookmarks::BookmarkNode::URL:
    case bookmarks::BookmarkNode::FOLDER:
      return node->GetTitle();
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
      return kBookmarkBarFolderName;
    case bookmarks::BookmarkNode::OTHER_NODE:
      return kOtherBookmarksFolderName;
    case bookmarks::BookmarkNode::MOBILE:
      return kMobileBookmarksFolderName;
  }
  NOTREACHED();
}

std::u16string NodeTitleFromEntityData(const syncer::EntityData& entity_data) {
  if (entity_data.server_defined_unique_tag == kBookmarkBarTag) {
    return kBookmarkBarFolderName;
  } else if (entity_data.server_defined_unique_tag == kOtherBookmarksTag) {
    return kOtherBookmarksFolderName;
  } else if (entity_data.server_defined_unique_tag == kMobileBookmarksTag) {
    return kMobileBookmarksFolderName;
  } else {
    return NodeTitleFromSpecifics(entity_data.specifics.bookmark());
  }
}

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
Key GroupingKeyFromLocalData(const bookmarks::BookmarkNode* node,
                             std::u16string path);

template <typename Key>
Key GroupingKeyFromAccountData(const sync_pb::BookmarkSpecifics& specifics,
                               std::u16string path);

template <>
UrlAndTitle GroupingKeyFromLocalData(const bookmarks::BookmarkNode* node,
                                     std::u16string path) {
  UrlAndTitle key;
  key.url = node->url();
  key.title = node->GetTitle();
  // `path` ignored but required in signture for template code.
  return key;
}

template <>
UrlAndTitle GroupingKeyFromAccountData(
    const sync_pb::BookmarkSpecifics& specifics,
    std::u16string path) {
  UrlAndTitle key;
  key.url = GURL(specifics.url());
  key.title = NodeTitleFromSpecifics(specifics);
  // `path` ignored but required in signture for template code.
  return key;
}

template <>
UrlAndUuid GroupingKeyFromLocalData(const bookmarks::BookmarkNode* node,
                                    std::u16string path) {
  UrlAndUuid key;
  key.url = node->url();
  key.uuid = node->uuid();
  // `path` ignored but required in signture for template code.
  return key;
}

template <>
UrlAndUuid GroupingKeyFromAccountData(
    const sync_pb::BookmarkSpecifics& specifics,
    std::u16string path) {
  UrlAndUuid key;
  key.url = GURL(specifics.url());
  key.uuid = base::Uuid::ParseLowercase(specifics.guid());
  // `path` ignored but required in signture for template code.
  return key;
}

template <>
UrlAndTitleAndPath GroupingKeyFromLocalData(const bookmarks::BookmarkNode* node,
                                            std::u16string path) {
  UrlAndTitleAndPath key;
  key.url = node->url();
  key.title = node->GetTitle();
  key.path = std::move(path);
  return key;
}

template <>
UrlAndTitleAndPath GroupingKeyFromAccountData(
    const sync_pb::BookmarkSpecifics& specifics,
    std::u16string path) {
  UrlAndTitleAndPath key;
  key.url = GURL(specifics.url());
  key.title = NodeTitleFromSpecifics(specifics);
  key.path = std::move(path);
  return key;
}

// Given two sets `set1` and `set2`, returns the number of values that exist in
// both.
template <typename Key>
size_t GetSetIntersectionSize(const base::flat_set<Key>& set1,
                              const base::flat_set<Key>& set2) {
  size_t intersection_size = 0;
  auto it1 = set1.begin();
  auto it2 = set2.begin();
  while (it1 != set1.end() && it2 != set2.end()) {
    if (*it1 == *it2) {
      ++intersection_size;
      ++it1;
      ++it2;
    } else if (*it1 < *it2) {
      ++it1;
    } else {
      ++it2;
    }
  }
  // The implementation above could be replaced the size resulting of calling
  // base::STLSetIntersection(), but this is avoided for performance reasons,
  // because base::STLSetIntersection() would compute a full set.
  DCHECK_EQ(intersection_size,
            base::STLSetIntersection<base::flat_set<Key>>(set1, set2).size());
  return intersection_size;
}

// Given two sets `account_data` and `local_data`, it returns how the two
// relate to each other as represented in enum `SetComparisonOutcome`.
template <typename Key>
SetComparisonOutcome CompareSets(const base::flat_set<Key>& account_data,
                                 const base::flat_set<Key>& local_data) {
  const size_t account_data_set_size = account_data.size();
  const size_t local_data_set_size = local_data.size();
  const size_t intersection_size =
      GetSetIntersectionSize(local_data, account_data);

  CHECK_LE(intersection_size, local_data_set_size);
  CHECK_LE(intersection_size, account_data_set_size);

  if (local_data_set_size == 0 && account_data_set_size == 0) {
    return SetComparisonOutcome::kBothEmpty;
  } else if (local_data_set_size == 0) {
    return SetComparisonOutcome::kLocalDataEmpty;
  } else if (account_data_set_size == 0) {
    return SetComparisonOutcome::kAccountDataEmpty;
  } else if (local_data_set_size == intersection_size) {
    if (account_data_set_size == intersection_size) {
      return SetComparisonOutcome::kExactMatchNonEmpty;
    } else {
      return SetComparisonOutcome::kLocalDataIsStrictSubsetOfAccountData;
    }
  }

  // Produce a notion of similarity based on which fraction of the total data
  // items (union) is included in the intersection.
  const size_t union_size =
      local_data_set_size + account_data_set_size - intersection_size;
  DCHECK_EQ(
      union_size,
      base::STLSetUnion<base::flat_set<Key>>(account_data, local_data).size());

  CHECK_LT(intersection_size, union_size);
  const double intersecting_fraction = 1.0 * intersection_size / union_size;

  if (intersecting_fraction >= 0.99) {
    return SetComparisonOutcome::kIntersectionBetween99And100Percent;
  } else if (intersecting_fraction >= 0.95) {
    return SetComparisonOutcome::kIntersectionBetween95And99Percent;
  } else if (intersecting_fraction >= 0.90) {
    return SetComparisonOutcome::kIntersectionBetween90And95Percent;
  } else if (intersecting_fraction >= 0.50) {
    return SetComparisonOutcome::kIntersectionBetween50And90Percent;
  } else if (intersecting_fraction >= 0.10) {
    return SetComparisonOutcome::kIntersectionBetween10And50Percent;
  } else if (intersecting_fraction > 0) {
    return SetComparisonOutcome::kIntersectionBelow10PercentExcludingZero;
  } else {
    return SetComparisonOutcome::kIntersectionEmpty;
  }
}

// Recursive function used to implement `ExtractLocalDataSet()` below.
template <typename Key>
void ExtractLocalDataSetRecursive(std::u16string path,
                                  const bookmarks::BookmarkNode* node,
                                  std::vector<Key>& keys) {
  CHECK(node);
  if (node->is_url()) {
    keys.emplace_back(GroupingKeyFromLocalData<Key>(node, std::move(path)));
    return;
  }
  for (const std::unique_ptr<bookmarks::BookmarkNode>& child :
       node->children()) {
    ExtractLocalDataSetRecursive(
        base::StrCat({path, u"/", GetBookmarkNodeTitle(node)}), child.get(),
        keys);
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
    ExtractLocalDataSetRecursive(/*path=*/u"", node, keys);
  }
  return base::flat_set<Key>(std::move(keys));
}

// Recursive function used to implement `ExtractAccountDataSet()` below.
template <typename Key>
void ExtractAccountDataSetRecursive(std::u16string path,
                                    const RemoteTreeNode& node,
                                    std::vector<Key>& keys) {
  if (node.entity().specifics.bookmark().type() ==
      sync_pb::BookmarkSpecifics::URL) {
    keys.emplace_back(GroupingKeyFromAccountData<Key>(
        node.entity().specifics.bookmark(), std::move(path)));
    return;
  }
  for (const RemoteTreeNode& child : node.children()) {
    ExtractAccountDataSetRecursive(
        base::StrCat({path, u"/", NodeTitleFromEntityData(node.entity())}),
        child, keys);
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
    ExtractAccountDataSetRecursive(/*path=*/u"", *node, keys);
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

base::flat_set<UrlAndTitleAndPath>
ExtractUniqueLocalNodesByUrlAndTitleAndPathForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection) {
  return ExtractLocalDataSet<UrlAndTitleAndPath>(
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

base::flat_set<UrlAndTitleAndPath>
ExtractUniqueAccountNodesByUrlAndTitleAndPathForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection) {
  return ExtractAccountDataSet<UrlAndTitleAndPath>(
      GetRelevantAccountSubtrees(all_account_data, subtree_selection));
}

SetComparisonOutcome CompareSetsForTesting(
    const base::flat_set<int>& account_data,
    const base::flat_set<int>& local_data) {
  return CompareSets<int>(account_data, local_data);
}

}  // namespace sync_bookmarks::metrics

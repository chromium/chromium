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
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/previously_syncing_gaia_id_info_for_metrics.h"
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

// Enum representing the number of URL bookmarks stored locally, bucketized into
// a few notable ranges. This enum is used as suffix when recording metrics.
enum BookmarkCountSuffix {
  kZeroLocalUrlBookmarks,
  kBetween1And19LocalUrlBookmarks,
  kBetween20and999LocalUrlBookmarks,
  k1000OrMoreLocalUrlBookmarks,
};

std::string_view SubtreeSelectionToInfix(SubtreeSelection value) {
  // LINT.IfChange(BookmarkComparisonSubtreeSelection)
  switch (value) {
    case SubtreeSelection::kConsideringAllBookmarks:
      return "ConsideringAllBookmarks";
    case SubtreeSelection::kUnderBookmarksBar:
      return "UnderBookmarksBar";
  }
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/histograms.xml:BookmarkComparisonSubtreeSelection)
  NOTREACHED();
}

std::string_view GroupingKeyInfixToString(GroupingKeyInfix value) {
  // LINT.IfChange(BookmarkComparisonGroupingKey)
  switch (value) {
    case GroupingKeyInfix::kByUrl:
      return "ByUrl";
    case GroupingKeyInfix::kByUrlAndTitle:
      return "ByUrlAndTitle";
    case GroupingKeyInfix::kByUrlAndUuid:
      return "ByUrlAndUuid";
    case GroupingKeyInfix::kByUrlAndTitleAndPath:
      return "ByUrlAndTitleAndPath";
    case GroupingKeyInfix::kByUrlAndTitleAndPathAndUuid:
      return "ByUrlAndTitleAndPathAndUuid";
  }
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/histograms.xml:BookmarkComparisonGroupingKey)
  NOTREACHED();
}

std::string_view BookmarkCountSuffixToString(BookmarkCountSuffix value) {
  // LINT.IfChange(BookmarkComparisonBookmarkCount)
  switch (value) {
    case kZeroLocalUrlBookmarks:
      return ".ZeroLocalUrlBookmarks";
    case kBetween1And19LocalUrlBookmarks:
      return ".Between1And19LocalUrlBookmarks";
    case kBetween20and999LocalUrlBookmarks:
      return ".Between20and999LocalUrlBookmarks";
    case k1000OrMoreLocalUrlBookmarks:
      return ".1000OrMoreLocalUrlBookmarks";
  }
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/histograms.xml:BookmarkComparisonBookmarkCount)
  NOTREACHED();
}

std::string_view PreviouslySyncingGaiaIdInfoToInfix(
    syncer::PreviouslySyncingGaiaIdInfoForMetrics value) {
  // LINT.IfChange(BookmarkComparisonPreviouslySyncingGaiaId)
  switch (value) {
    case syncer::PreviouslySyncingGaiaIdInfoForMetrics::kUnspecified:
      NOTREACHED();
    case syncer::PreviouslySyncingGaiaIdInfoForMetrics::
        kSyncFeatureNeverPreviouslyTurnedOn:
      return ".NoPreviousGaiaId";
    case syncer::PreviouslySyncingGaiaIdInfoForMetrics::
        kCurrentGaiaIdMatchesPreviousWithSyncFeatureOn:
      return ".MatchesPreviousGaiaId";
    case syncer::PreviouslySyncingGaiaIdInfoForMetrics::
        kCurrentGaiaIdIfDiffersPreviousWithSyncFeatureOn:
      return ".DiffersPreviousGaiaId";
  }
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/histograms.xml:BookmarkComparisonPreviouslySyncingGaiaId)
  NOTREACHED();
}

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
    case SubtreeSelection::kUnderBookmarksBar:
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
    case SubtreeSelection::kUnderBookmarksBar:
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

size_t CountUrlNodesInSubtree(const bookmarks::BookmarkNode* node) {
  CHECK(node);
  if (node->is_url()) {
    return 1;
  }
  size_t result = 0;
  for (const std::unique_ptr<bookmarks::BookmarkNode>& child :
       node->children()) {
    result += CountUrlNodesInSubtree(child.get());
  }
  return result;
}

BookmarkCountSuffix CountLocalBookmarks(
    const std::vector<const bookmarks::BookmarkNode*>& local_data) {
  size_t num_url_nodes = 0;
  for (const bookmarks::BookmarkNode* permanent_node : local_data) {
    num_url_nodes += CountUrlNodesInSubtree(permanent_node);
  }
  if (num_url_nodes == 0) {
    return kZeroLocalUrlBookmarks;
  }
  if (num_url_nodes < 20) {
    return kBetween1And19LocalUrlBookmarks;
  }
  if (num_url_nodes < 1000) {
    return kBetween20and999LocalUrlBookmarks;
  }
  return k1000OrMoreLocalUrlBookmarks;
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
UrlOnly GroupingKeyFromLocalData(const bookmarks::BookmarkNode* node,
                                 std::u16string path) {
  UrlOnly key;
  key.url = node->url();
  // `path` ignored but required in signture for template code.
  return key;
}

template <>
UrlOnly GroupingKeyFromAccountData(const sync_pb::BookmarkSpecifics& specifics,
                                   std::u16string path) {
  UrlOnly key;
  key.url = GURL(specifics.url());
  // `path` ignored but required in signture for template code.
  return key;
}

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

template <>
UrlAndTitleAndPathAndUuid GroupingKeyFromLocalData(
    const bookmarks::BookmarkNode* node,
    std::u16string path) {
  UrlAndTitleAndPathAndUuid key;
  key.url = node->url();
  key.title = node->GetTitle();
  key.path = std::move(path);
  key.uuid = node->uuid();
  return key;
}

template <>
UrlAndTitleAndPathAndUuid GroupingKeyFromAccountData(
    const sync_pb::BookmarkSpecifics& specifics,
    std::u16string path) {
  UrlAndTitleAndPathAndUuid key;
  key.url = GURL(specifics.url());
  key.title = NodeTitleFromSpecifics(specifics);
  key.path = std::move(path);
  key.uuid = base::Uuid::ParseLowercase(specifics.guid());
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

template <typename Key>
void CompareAndLogHistogramsWithKey(
    SubtreeSelection subtree_selection,
    syncer::PreviouslySyncingGaiaIdInfoForMetrics
        previously_syncing_gaia_id_info,
    BookmarkCountSuffix bookmark_count_suffix,
    const std::vector<const bookmarks::BookmarkNode*>& relevant_local_subtrees,
    const std::vector<const RemoteTreeNode*>& relevant_account_subtrees) {
  const base::flat_set<Key> local_data_set =
      ExtractLocalDataSet<Key>(relevant_local_subtrees);
  const base::flat_set<Key> account_data_set =
      ExtractAccountDataSet<Key>(relevant_account_subtrees);

  // When recording the metric, always record four metrics, resulting from
  // the combinatorial cases for:
  // 1. With and without the infix representing
  //    PreviouslySyncingGaiaIdInfoForMetrics.
  // 2. With and without the suffix representing the number of local URL
  //    bookmarks.
  for (std::string_view optional_previously_syncing_gaia_id_info_infix :
       {std::string_view(),
        PreviouslySyncingGaiaIdInfoToInfix(previously_syncing_gaia_id_info)}) {
    for (std::string_view optional_bookmark_count_suffix :
         {std::string_view(),
          BookmarkCountSuffixToString(bookmark_count_suffix)}) {
      const std::string legacy_histogram_name =
          base::StrCat({"Sync.BookmarkModelMerger.Comparison",
                        optional_previously_syncing_gaia_id_info_infix, ".",
                        SubtreeSelectionToInfix(subtree_selection), ".",
                        GroupingKeyInfixToString(Key::kGroupingKeyInfix),
                        optional_bookmark_count_suffix});

      // The call below to CompareSets() mixes up local data with account data.
      // Such implementation was accidental, but the resulting metric is anyway
      // meaningful, and the resulting behavior documented in histograms.xml.
      // For a fixed version of this, see `fixed_histogram_name` immediately
      // below.
      // TODO(crbug.com/424551547): Clean up the legacy metric, if not both,
      // in upcoming milestones (M139 or M140).
      base::UmaHistogramEnumeration(
          legacy_histogram_name, CompareSets(/*account_data=*/local_data_set,
                                             /*local_data=*/account_data_set));

      const std::string fixed_histogram_name =
          base::StrCat({"Sync.BookmarkModelMerger.Comparison2",
                        optional_previously_syncing_gaia_id_info_infix, ".",
                        SubtreeSelectionToInfix(subtree_selection), ".",
                        GroupingKeyInfixToString(Key::kGroupingKeyInfix),
                        optional_bookmark_count_suffix});

      base::UmaHistogramEnumeration(
          fixed_histogram_name, CompareSets(/*account_data=*/account_data_set,
                                            /*local_data=*/local_data_set));
    }
  }
}

}  // namespace

UrlAndTitleAndPathAndUuid::UrlAndTitleAndPathAndUuid() = default;

UrlAndTitleAndPathAndUuid::UrlAndTitleAndPathAndUuid(
    const GURL& url,
    const std::u16string& title,
    const std::u16string& path,
    const base::Uuid& uuid)
    : url(url), title(title), path(path), uuid(uuid) {}

UrlAndTitleAndPathAndUuid::UrlAndTitleAndPathAndUuid(
    const UrlAndTitleAndPathAndUuid&) = default;

UrlAndTitleAndPathAndUuid::UrlAndTitleAndPathAndUuid(
    UrlAndTitleAndPathAndUuid&&) = default;

UrlAndTitleAndPathAndUuid::~UrlAndTitleAndPathAndUuid() = default;

UrlAndTitleAndPathAndUuid& UrlAndTitleAndPathAndUuid::operator=(
    const UrlAndTitleAndPathAndUuid&) = default;

UrlAndTitleAndPathAndUuid& UrlAndTitleAndPathAndUuid::operator=(
    UrlAndTitleAndPathAndUuid&&) = default;

base::flat_set<UrlOnly> ExtractUniqueLocalNodesByUrlForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection) {
  return ExtractLocalDataSet<UrlOnly>(
      GetRelevantLocalSubtrees(all_local_data, subtree_selection));
}

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

base::flat_set<UrlAndTitleAndPathAndUuid>
ExtractUniqueLocalNodesByUrlAndTitleAndPathAndUuidForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection) {
  return ExtractLocalDataSet<UrlAndTitleAndPathAndUuid>(
      GetRelevantLocalSubtrees(all_local_data, subtree_selection));
}

base::flat_set<UrlOnly> ExtractUniqueAccountNodesByUrlForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection) {
  return ExtractAccountDataSet<UrlOnly>(
      GetRelevantAccountSubtrees(all_account_data, subtree_selection));
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

base::flat_set<UrlAndTitleAndPathAndUuid>
ExtractUniqueAccountNodesByUrlAndTitleAndPathAndUuidForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection) {
  return ExtractAccountDataSet<UrlAndTitleAndPathAndUuid>(
      GetRelevantAccountSubtrees(all_account_data, subtree_selection));
}

SetComparisonOutcome CompareSetsForTesting(
    const base::flat_set<int>& account_data,
    const base::flat_set<int>& local_data) {
  return CompareSets<int>(account_data, local_data);
}

void CompareBookmarkModelAndLogHistograms(
    const BookmarkModelView& all_local_data,
    const BookmarkModelMerger::RemoteForest& all_account_data,
    syncer::PreviouslySyncingGaiaIdInfoForMetrics
        previously_syncing_gaia_id_info) {
  CHECK_NE(previously_syncing_gaia_id_info,
           syncer::PreviouslySyncingGaiaIdInfoForMetrics::kUnspecified);

  for (SubtreeSelection subtree_selection :
       {SubtreeSelection::kConsideringAllBookmarks,
        SubtreeSelection::kUnderBookmarksBar}) {
    const std::vector<const bookmarks::BookmarkNode*> relevant_local_subtrees =
        GetRelevantLocalSubtrees(all_local_data, subtree_selection);
    const std::vector<const RemoteTreeNode*> relevant_account_subtrees =
        GetRelevantAccountSubtrees(all_account_data, subtree_selection);

    const BookmarkCountSuffix bookmark_count_suffix =
        CountLocalBookmarks(relevant_local_subtrees);

    CompareAndLogHistogramsWithKey<UrlOnly>(
        subtree_selection, previously_syncing_gaia_id_info,
        bookmark_count_suffix, relevant_local_subtrees,
        relevant_account_subtrees);
    CompareAndLogHistogramsWithKey<UrlAndTitle>(
        subtree_selection, previously_syncing_gaia_id_info,
        bookmark_count_suffix, relevant_local_subtrees,
        relevant_account_subtrees);
    CompareAndLogHistogramsWithKey<UrlAndUuid>(
        subtree_selection, previously_syncing_gaia_id_info,
        bookmark_count_suffix, relevant_local_subtrees,
        relevant_account_subtrees);
    CompareAndLogHistogramsWithKey<UrlAndTitleAndPath>(
        subtree_selection, previously_syncing_gaia_id_info,
        bookmark_count_suffix, relevant_local_subtrees,
        relevant_account_subtrees);
    CompareAndLogHistogramsWithKey<UrlAndTitleAndPathAndUuid>(
        subtree_selection, previously_syncing_gaia_id_info,
        bookmark_count_suffix, relevant_local_subtrees,
        relevant_account_subtrees);
  }
}

}  // namespace sync_bookmarks::metrics

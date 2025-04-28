// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_COMPARISON_METRICS_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_COMPARISON_METRICS_H_

#include "base/containers/flat_set.h"
#include "base/uuid.h"
#include "components/sync/base/previously_syncing_gaia_id_info_for_metrics.h"
#include "components/sync_bookmarks/bookmark_model_merger.h"
#include "url/gurl.h"

namespace sync_bookmarks {

class BookmarkModelView;

namespace metrics {

// Enum representing which subtree of the bookmark model was used when
// comparing local bookmarks with account bookmarks. This enum is used as
// infix when recording metrics.
enum class SubtreeSelection {
  kConsideringAllBookmarks,
  kUnderBookmarksBar,
};

// Enum representing the method or criterion used as uniqueness key when
// comparing local bookmarks with account bookmarks. This enum is used as
// infix when recording metrics.
enum class GroupingKeyInfix {
  kByUrl,
  kByUrlAndTitle,
  kByUrlAndUuid,
  kByUrlAndTitleAndPath,
  kByUrlAndTitleAndPathAndUuid,
};

// Result of comparing two datasets for the purpose of logging metrics. Note
// that enum values listed first take precedence (are evaluated earlier) than
// those following.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with the homonym enum
// in tools/metrics/histograms/metadata/sync/enums.xml.
// Exposed in the header file for testing.
// LINT.IfChange(BookmarkSetComparisonOutcome)
enum class SetComparisonOutcome {
  kBothEmpty = 0,
  kLocalDataEmpty = 1,
  kAccountDataEmpty = 2,
  kExactMatchNonEmpty = 3,
  kLocalDataIsStrictSubsetOfAccountData = 4,
  kIntersectionBetween99And100Percent = 5,
  kIntersectionBetween95And99Percent = 6,
  kIntersectionBetween90And95Percent = 7,
  kIntersectionBetween50And90Percent = 8,
  kIntersectionBetween10And50Percent = 9,
  kIntersectionBelow10PercentExcludingZero = 10,
  kIntersectionEmpty = 11,
  kMaxValue = kIntersectionEmpty
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:BookmarkSetComparisonOutcome)

struct UrlOnly {
  static constexpr GroupingKeyInfix kGroupingKeyInfix =
      GroupingKeyInfix::kByUrl;

  auto operator<=>(const UrlOnly&) const = default;

  GURL url;
};

struct UrlAndTitle {
  static constexpr GroupingKeyInfix kGroupingKeyInfix =
      GroupingKeyInfix::kByUrlAndTitle;

  auto operator<=>(const UrlAndTitle&) const = default;

  GURL url;
  std::u16string title;
};

struct UrlAndUuid {
  static constexpr GroupingKeyInfix kGroupingKeyInfix =
      GroupingKeyInfix::kByUrlAndUuid;

  auto operator<=>(const UrlAndUuid&) const = default;

  GURL url;
  base::Uuid uuid;
};

struct UrlAndTitleAndPath {
  static constexpr GroupingKeyInfix kGroupingKeyInfix =
      GroupingKeyInfix::kByUrlAndTitleAndPath;

  auto operator<=>(const UrlAndTitleAndPath&) const = default;

  GURL url;
  std::u16string title;
  // Ancestor folder titles concatenated with '/'.
  std::u16string path;
};

struct UrlAndTitleAndPathAndUuid {
  static constexpr GroupingKeyInfix kGroupingKeyInfix =
      GroupingKeyInfix::kByUrlAndTitleAndPathAndUuid;

  UrlAndTitleAndPathAndUuid();
  UrlAndTitleAndPathAndUuid(const GURL& url,
                            const std::u16string& title,
                            const std::u16string& path,
                            const base::Uuid& uuid);
  UrlAndTitleAndPathAndUuid(const UrlAndTitleAndPathAndUuid&);
  UrlAndTitleAndPathAndUuid(UrlAndTitleAndPathAndUuid&&);
  ~UrlAndTitleAndPathAndUuid();

  UrlAndTitleAndPathAndUuid& operator=(const UrlAndTitleAndPathAndUuid&);
  UrlAndTitleAndPathAndUuid& operator=(UrlAndTitleAndPathAndUuid&&);

  auto operator<=>(const UrlAndTitleAndPathAndUuid&) const = default;

  GURL url;
  std::u16string title;
  // Ancestor folder titles concatenated with '/'.
  std::u16string path;
  base::Uuid uuid;
};

// Test-only functions to verify the logic related to extracting local data.
base::flat_set<UrlOnly> ExtractUniqueLocalNodesByUrlForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndTitle> ExtractUniqueLocalNodesByUrlAndTitleForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndUuid> ExtractUniqueLocalNodesByUrlAndUuidForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndTitleAndPath>
ExtractUniqueLocalNodesByUrlAndTitleAndPathForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndTitleAndPathAndUuid>
ExtractUniqueLocalNodesByUrlAndTitleAndPathAndUuidForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection);

// Test-only functions to verify the logic related to extracting account data.
base::flat_set<UrlOnly> ExtractUniqueAccountNodesByUrlForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndTitle> ExtractUniqueAccountNodesByUrlAndTitleForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndUuid> ExtractUniqueAccountNodesByUrlAndUuidForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndTitleAndPath>
ExtractUniqueAccountNodesByUrlAndTitleAndPathForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndTitleAndPathAndUuid>
ExtractUniqueAccountNodesByUrlAndTitleAndPathAndUuidForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection);

// Test-only function to compare two sets.
SetComparisonOutcome CompareSetsForTesting(
    const base::flat_set<int>& account_data,
    const base::flat_set<int>& local_data);

void CompareBookmarkModelAndLogHistograms(
    const BookmarkModelView& all_local_data,
    const BookmarkModelMerger::RemoteForest& all_account_data,
    syncer::PreviouslySyncingGaiaIdInfoForMetrics
        previously_syncing_gaia_id_info);

}  // namespace metrics
}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_COMPARISON_METRICS_H_

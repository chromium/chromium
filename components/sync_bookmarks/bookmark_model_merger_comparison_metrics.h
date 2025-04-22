// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_COMPARISON_METRICS_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_COMPARISON_METRICS_H_

#include "base/containers/flat_set.h"
#include "base/uuid.h"
#include "components/sync_bookmarks/bookmark_model_merger.h"
#include "url/gurl.h"

namespace sync_bookmarks {

class BookmarkModelView;

namespace metrics {

enum class SubtreeSelection {
  kConsideringAllBookmarks,
  kUnderBookmarkBar,
};

struct UrlAndTitle {
  auto operator<=>(const UrlAndTitle&) const = default;

  GURL url;
  std::u16string title;
};

struct UrlAndUuid {
  auto operator<=>(const UrlAndUuid&) const = default;

  GURL url;
  base::Uuid uuid;
};

// Test-only functions to verify the logic related to extracting local data.
base::flat_set<UrlAndTitle> ExtractUniqueLocalNodesByUrlAndTitleForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndUuid> ExtractUniqueLocalNodesByUrlAndUuidForTesting(
    const BookmarkModelView& all_local_data,
    SubtreeSelection subtree_selection);

// Test-only functions to verify the logic related to extracting account data.
base::flat_set<UrlAndTitle> ExtractUniqueAccountNodesByUrlAndTitleForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection);
base::flat_set<UrlAndUuid> ExtractUniqueAccountNodesByUrlAndUuidForTesting(
    const BookmarkModelMerger::RemoteForest& all_account_data,
    SubtreeSelection subtree_selection);

}  // namespace metrics
}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_MERGER_COMPARISON_METRICS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_BOOKMARKS_URL_VISIT_AGGREGATES_TRANSFORMER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_BOOKMARKS_URL_VISIT_AGGREGATES_TRANSFORMER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/visited_url_ranking/public/url_visit.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"

namespace bookmarks {
class BookmarkModel;
}

namespace visited_url_ranking {

class BookmarksURLVisitAggregatesTransformer
    : public URLVisitAggregatesTransformer {
 public:
  explicit BookmarksURLVisitAggregatesTransformer(
      bookmarks::BookmarkModel* bookmark_model);
  ~BookmarksURLVisitAggregatesTransformer() override = default;

  // Disallow copy/assign.
  BookmarksURLVisitAggregatesTransformer(
      const BookmarksURLVisitAggregatesTransformer&) = delete;
  BookmarksURLVisitAggregatesTransformer& operator=(
      const BookmarksURLVisitAggregatesTransformer&) = delete;

  // URLVisitAggregatesTransformer:

  // Sets bookmark related fields for `URLVisitAggregate` objects (e.g.,
  // `is_bookmarked`).
  void Transform(std::vector<URLVisitAggregate> aggregates,
                 const FetchOptions& options,
                 OnTransformCallback callback) override;

 private:
  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_TRANSFORMER_BOOKMARKS_URL_VISIT_AGGREGATES_TRANSFORMER_H_

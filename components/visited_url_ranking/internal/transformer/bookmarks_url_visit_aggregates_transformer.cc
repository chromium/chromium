// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/bookmarks_url_visit_aggregates_transformer.h"

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

BookmarksURLVisitAggregatesTransformer::BookmarksURLVisitAggregatesTransformer(
    bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {
  CHECK(bookmark_model);
}

void BookmarksURLVisitAggregatesTransformer::Transform(
    std::vector<URLVisitAggregate> aggregates,
    const FetchOptions& options,
    OnTransformCallback callback) {
  for (auto& url_visit_aggregate : aggregates) {
    std::set<const GURL*> urls = url_visit_aggregate.GetAssociatedURLs();
    for (const auto* url : urls) {
      if (bookmark_model_->IsBookmarked(*url)) {
        url_visit_aggregate.bookmarked = true;
        continue;
      }
    }
  }

  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking

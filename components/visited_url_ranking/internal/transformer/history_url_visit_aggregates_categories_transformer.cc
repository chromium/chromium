// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_categories_transformer.h"

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/history/core/browser/history_types.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace {

bool AnnotationsContainsAnyOfCategories(
    const history::VisitContentAnnotations& content_annotations,
    const base::flat_set<std::string>& categories) {
  for (const auto& visit_category :
       content_annotations.model_annotations.categories) {
    if (categories.contains(visit_category.id)) {
      return true;
    }
  }
  return false;
}

}  // namespace

namespace visited_url_ranking {

HistoryURLVisitAggregatesCategoriesTransformer::
    HistoryURLVisitAggregatesCategoriesTransformer(
        base::flat_set<std::string> blocklisted_categories)
    : blocklisted_categories_(std::move(blocklisted_categories)) {}

HistoryURLVisitAggregatesCategoriesTransformer::
    ~HistoryURLVisitAggregatesCategoriesTransformer() = default;

void HistoryURLVisitAggregatesCategoriesTransformer::Transform(
    std::vector<URLVisitAggregate> aggregates,
    const FetchOptions& options,
    OnTransformCallback callback) {
  std::erase_if(aggregates, [&](auto& visit_aggregate) {
    const auto& it = visit_aggregate.fetcher_data_map.find(Fetcher::kHistory);
    if (it == visit_aggregate.fetcher_data_map.end()) {
      return false;
    }

    const auto& history =
        std::get_if<URLVisitAggregate::HistoryData>(&it->second);
    if (!history) {
      return false;
    }

    return AnnotationsContainsAnyOfCategories(
        history->last_visited.content_annotations, blocklisted_categories_);
  });

  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_visibility_score_transformer.h"

#include <vector>

#include "base/functional/callback.h"
#include "components/history/core/browser/history_types.h"
#include "components/visited_url_ranking/public/url_visit.h"

namespace visited_url_ranking {

HistoryURLVisitAggregatesVisibilityScoreTransformer::
    HistoryURLVisitAggregatesVisibilityScoreTransformer(float visibility_score)
    : visibility_score_threshold_(visibility_score) {}

void HistoryURLVisitAggregatesVisibilityScoreTransformer::Transform(
    std::vector<URLVisitAggregate> aggregates,
    const FetchOptions& options,
    OnTransformCallback callback) {
  std::erase_if(aggregates, [&](auto& visit_aggregate) {
    const auto& it = visit_aggregate.fetcher_data_map.find(Fetcher::kHistory);
    if (it == visit_aggregate.fetcher_data_map.end()) {
      VLOG(2) << "History visibility filter missing history visit "
              << visit_aggregate.url_key;
      return true;
    }

    const auto history =
        std::get_if<URLVisitAggregate::HistoryData>(&it->second);
    if (!history) {
      return false;
    }

    bool below_threshold =
        history->last_visited.content_annotations.model_annotations
            .visibility_score < visibility_score_threshold_;
    VLOG_IF(2, below_threshold) << "History visibility filter below threshold "
                                << visit_aggregate.url_key;
    return below_threshold;
  });

  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking

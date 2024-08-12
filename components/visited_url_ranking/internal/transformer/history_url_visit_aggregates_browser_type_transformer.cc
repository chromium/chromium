// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/history_url_visit_aggregates_browser_type_transformer.h"

namespace visited_url_ranking {

HistoryURLVisitAggregatesBrowserTypeTransformer::
    ~HistoryURLVisitAggregatesBrowserTypeTransformer() = default;

void HistoryURLVisitAggregatesBrowserTypeTransformer::Transform(
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

    // Entries from AuthView are for authentication, and should be filtered out.
    return history->last_visited.context_annotations.on_visit.browser_type ==
           history::VisitContextAnnotations::BrowserType::kAuthTab;
  });

  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking

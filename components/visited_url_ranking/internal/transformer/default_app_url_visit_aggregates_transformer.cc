// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/default_app_url_visit_aggregates_transformer.h"

namespace visited_url_ranking {

DefaultAppURLVisitAggregatesTransformer::
    DefaultAppURLVisitAggregatesTransformer(
        base::flat_set<std::string_view> default_app_blocklist)
    : default_app_blocklist_(default_app_blocklist) {}

DefaultAppURLVisitAggregatesTransformer::
    ~DefaultAppURLVisitAggregatesTransformer() = default;

void DefaultAppURLVisitAggregatesTransformer::Transform(
    std::vector<URLVisitAggregate> aggregates,
    OnTransformCallback callback) {
  std::erase_if(aggregates, [&](auto& visit_aggregate) {
    for (const auto& fetcher_entry : visit_aggregate.fetcher_data_map) {
      const auto& tab_data =
          std::get_if<URLVisitAggregate::TabData>(&fetcher_entry.second);
      const GURL& url = tab_data->last_active_tab.visit.url;
      return default_app_blocklist_.contains(url.host());
    }
    return false;
  });
  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking

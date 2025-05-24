// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/transformer/default_app_url_visit_aggregates_transformer.h"

#include <utility>

#include "components/visited_url_ranking/public/url_visit.h"

namespace {

using visited_url_ranking::URLVisitAggregate;

const GURL& GetVisitVariantUrl(
    const URLVisitAggregate::URLVisitVariant& visit_variant) {
  return std::visit(
      visited_url_ranking::URLVisitVariantHelper{
          [&](const URLVisitAggregate::TabData& tab_data) -> const GURL& {
            return tab_data.last_active_tab.visit.url;
          },
          [&](const URLVisitAggregate::HistoryData& history_data)
              -> const GURL& {
            return history_data.last_visited.url_row.url();
          }},
      visit_variant);
}

}  // namespace

namespace visited_url_ranking {

DefaultAppURLVisitAggregatesTransformer::
    DefaultAppURLVisitAggregatesTransformer(
        base::flat_set<std::string_view> default_app_blocklist)
    : default_app_blocklist_(std::move(default_app_blocklist)) {}

DefaultAppURLVisitAggregatesTransformer::
    ~DefaultAppURLVisitAggregatesTransformer() = default;

void DefaultAppURLVisitAggregatesTransformer::Transform(
    std::vector<URLVisitAggregate> aggregates,
    const FetchOptions& options,
    OnTransformCallback callback) {
  std::erase_if(aggregates, [&](auto& visit_aggregate) {
    for (const auto& fetcher_entry : visit_aggregate.fetcher_data_map) {
      const GURL& url = GetVisitVariantUrl(fetcher_entry.second);
      return default_app_blocklist_.contains(url.host());
    }
    return false;
  });
  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking

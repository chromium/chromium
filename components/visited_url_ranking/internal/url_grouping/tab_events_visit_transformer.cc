// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/tab_events_visit_transformer.h"

namespace visited_url_ranking {

TabEventsVisitTransformer::TabEventsVisitTransformer()
    : tab_tracker_(nullptr) {}

TabEventsVisitTransformer::~TabEventsVisitTransformer() = default;

void TabEventsVisitTransformer::Transform(
    std::vector<URLVisitAggregate> aggregates,
    const FetchOptions& options,
    OnTransformCallback callback) {
  for (auto& candidate : aggregates) {
    auto tab_data_it = candidate.fetcher_data_map.find(Fetcher::kTabModel);
    auto* tab = std::get_if<URLVisitAggregate::TabData>(&tab_data_it->second);
    tab->recent_fg_count =
        tab_tracker_->GetSelectedCount(tab->last_active_tab.id);
  }
  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking

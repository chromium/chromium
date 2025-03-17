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
  // TODO: Attach signals about each tab from `tab_tracker`and return the list.
  std::move(callback).Run(Status::kSuccess, std::move(aggregates));
}

}  // namespace visited_url_ranking

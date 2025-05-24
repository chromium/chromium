// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_TAB_EVENTS_VISIT_TRANSFORMER_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_TAB_EVENTS_VISIT_TRANSFORMER_H_

#include "components/visited_url_ranking/internal/url_grouping/tab_event_tracker_impl.h"
#include "components/visited_url_ranking/public/url_visit_aggregates_transformer.h"

namespace visited_url_ranking {

// Visited URL service transformer that adds data about tab events to the
// candidates when fetching all signals.
class TabEventsVisitTransformer : public URLVisitAggregatesTransformer {
 public:
  TabEventsVisitTransformer();
  ~TabEventsVisitTransformer() override;

  TabEventsVisitTransformer(const TabEventsVisitTransformer&) = delete;
  TabEventsVisitTransformer& operator=(const TabEventsVisitTransformer&) =
      delete;

  // URLVisitAggregatesTransformer impl:
  void Transform(std::vector<URLVisitAggregate> aggregates,
                 const FetchOptions& options,
                 OnTransformCallback callback) override;

  void set_tab_event_tracker(TabEventTrackerImpl* tab_tracker) {
    tab_tracker_ = tab_tracker;
  }

 private:
  raw_ptr<TabEventTrackerImpl> tab_tracker_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_TAB_EVENTS_VISIT_TRANSFORMER_H_

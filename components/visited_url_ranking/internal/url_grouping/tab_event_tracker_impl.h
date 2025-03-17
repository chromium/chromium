// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_TAB_EVENT_TRACKER_IMPL_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_TAB_EVENT_TRACKER_IMPL_H_

#include "base/functional/callback_forward.h"
#include "components/visited_url_ranking/public/url_grouping/tab_event_tracker.h"

namespace visited_url_ranking {

// TabEventTracker implementation that sends events to storage and triggers
// suggestion computation.
class TabEventTrackerImpl : public TabEventTracker {
 public:
  using OnNewEventCallback = base::RepeatingClosure;
  explicit TabEventTrackerImpl(OnNewEventCallback on_new_event_callback);
  ~TabEventTrackerImpl() override;

  TabEventTrackerImpl(const TabEventTrackerImpl&) = delete;
  TabEventTrackerImpl& operator=(const TabEventTrackerImpl&) = delete;

  // TabEventTracker impl:
  void DidAddTab(int tab_id) override;
  void DidSelectTab(int tab_id, TabSelectionType tab_selection_type) override;
  void DidMoveTab(int tab_id, int new_index, int current_index) override;

 private:
  OnNewEventCallback on_new_event_callback_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_TAB_EVENT_TRACKER_IMPL_H_

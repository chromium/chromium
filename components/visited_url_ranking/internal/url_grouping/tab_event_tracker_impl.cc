// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/tab_event_tracker_impl.h"

namespace visited_url_ranking {

TabEventTrackerImpl::TabEventTrackerImpl(
    OnNewEventCallback on_new_event_callback)
    : on_new_event_callback_(on_new_event_callback) {}
TabEventTrackerImpl::~TabEventTrackerImpl() = default;

void TabEventTrackerImpl::DidAddTab(int tab_id, int tab_launch_type) {
  // TODO(ssid): Observe tab closures to reset the counters.
  tab_id_to_selected_count_[tab_id]++;
  on_new_event_callback_.Run();
}

void TabEventTrackerImpl::DidSelectTab(int tab_id,
                                       TabSelectionType tab_selection_type,
                                       int last_tab_id) {
  // TODO(ssid): Only increment for user triggered selection.
  tab_id_to_selected_count_[tab_id]++;
  on_new_event_callback_.Run();
}

void TabEventTrackerImpl::DidMoveTab(int tab_id,
                                     int new_index,
                                     int current_index) {}

void TabEventTrackerImpl::DidEnterTabSwitcher() {}

int TabEventTrackerImpl::GetSelectedCount(int tab_id) const {
  if (tab_id_to_selected_count_.contains(tab_id)) {
    return tab_id_to_selected_count_.at(tab_id);
  }
  return 0;
}

}  // namespace visited_url_ranking

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
  on_new_event_callback_.Run();
}

void TabEventTrackerImpl::DidSelectTab(int tab_id,
                                       TabSelectionType tab_selection_type,
                                       int last_tab_id) {
  on_new_event_callback_.Run();
}

void TabEventTrackerImpl::DidMoveTab(int tab_id,
                                     int new_index,
                                     int current_index) {
  on_new_event_callback_.Run();
}

void TabEventTrackerImpl::DidEnterTabSwitcher() {
  on_new_event_callback_.Run();
}

}  // namespace visited_url_ranking

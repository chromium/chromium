// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/tab_event_tracker_impl.h"

namespace visited_url_ranking {

namespace {

const base::TimeDelta kSelectionTimeWindow = base::Minutes(30);

}  // namespace

TabEventTrackerImpl::TabEventTrackerImpl(
    OnNewEventCallback on_new_event_callback)
    : on_new_event_callback_(on_new_event_callback) {}
TabEventTrackerImpl::~TabEventTrackerImpl() = default;

TabEventTrackerImpl::TabSelection::TabSelection(
    int tab_id,
    TabSelectionType tab_selection_type,
    base::Time time)
    : tab_id(tab_id), tab_selection_type(tab_selection_type), time(time) {}
TabEventTrackerImpl::TabSelection::~TabSelection() = default;

void TabEventTrackerImpl::DidAddTab(int tab_id, int tab_launch_type) {
  on_new_event_callback_.Run();
}

void TabEventTrackerImpl::DidSelectTab(int tab_id,
                                       TabSelectionType tab_selection_type,
                                       int last_tab_id) {
  if (tab_selection_type != TabSelectionType::kFromUser ||
      last_tab_id == tab_id) {
    return;
  }
  tab_id_selection_map_[tab_id].emplace_back(tab_id, tab_selection_type,
                                             base::Time::Now());
  on_new_event_callback_.Run();
}

void TabEventTrackerImpl::WillCloseTab(int tab_id) {
  closing_tabs_.insert(tab_id);
}

void TabEventTrackerImpl::TabClosureUndone(int tab_id) {
  closing_tabs_.erase(tab_id);
}

void TabEventTrackerImpl::TabClosureCommitted(int tab_id) {
  closing_tabs_.erase(tab_id);
  tab_id_selection_map_.erase(tab_id);
}

void TabEventTrackerImpl::DidMoveTab(int tab_id,
                                     int new_index,
                                     int current_index) {}

void TabEventTrackerImpl::OnPageLoadFinished(int tab_id) {
  on_new_event_callback_.Run();
}

void TabEventTrackerImpl::DidEnterTabSwitcher() {}

int TabEventTrackerImpl::GetSelectedCount(int tab_id) const {
  if (!closing_tabs_.contains(tab_id) &&
      tab_id_selection_map_.contains(tab_id)) {
    std::vector<TabSelection> selection_list = tab_id_selection_map_.at(tab_id);
    std::erase_if(selection_list, [&](TabSelection selection) {
      return base::Time::Now() - selection.time > kSelectionTimeWindow;
    });
    return selection_list.size();
  }
  return 0;
}

}  // namespace visited_url_ranking

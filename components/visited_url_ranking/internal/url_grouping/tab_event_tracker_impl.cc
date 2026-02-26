// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visited_url_ranking/internal/url_grouping/tab_event_tracker_impl.h"

#include "components/visited_url_ranking/public/features.h"

namespace visited_url_ranking {

namespace {

const base::TimeDelta kSelectionTimeWindow = base::Minutes(10);

}  // namespace

const char TabEventTrackerImpl::kAndroidNativeNewTabPageURL[] =
    "chrome-native://newtab/";

TabEventTrackerImpl::TabEventTrackerImpl(
    OnNewEventCallback trigger_event_callback,
    OnNewEventCallback invalidate_cache_callback)
    : trigger_event_callback_(trigger_event_callback),
      invalidate_cache_callback_(invalidate_cache_callback),
      tab_switcher_trigger_only_(
          features::kGroupSuggestionEnableTabSwitcherOnly.Get()),
      trigger_on_navigation_(
          features::kGroupSuggestionEnableRecentlyOpened.Get() ||
          features::kGroupSuggestionEnableSameOrigin.Get() ||
          features::kGroupSuggestionTriggerCalculationOnPageLoad.Get()) {}
TabEventTrackerImpl::~TabEventTrackerImpl() = default;

TabEventTrackerImpl::TabSelection::TabSelection() = default;
TabEventTrackerImpl::TabSelection::TabSelection(
    int tab_id,
    TabSelectionCause tab_selection_cause,
    base::Time time)
    : tab_id(tab_id), tab_selection_cause(tab_selection_cause), time(time) {}
TabEventTrackerImpl::TabSelection::~TabSelection() = default;

void TabEventTrackerImpl::DidAddTab(int tab_id, int tab_launch_type) {
  // The cache could have suggestions without the opened tab, which may need
  // to be included.
  invalidate_cache_callback_.Run();
  if (!tab_switcher_trigger_only_) {
    trigger_event_callback_.Run();
  }
}

void TabEventTrackerImpl::DidSelectTab(int tab_id,
                                       const GURL& url,
                                       TabSelectionCause tab_selection_cause,
                                       int last_tab_id) {
  current_selection_ =
      TabSelection(tab_id, tab_selection_cause, base::Time::Now());
  if ((tab_selection_cause != TabSelectionCause::kFromUser &&
       tab_selection_cause != TabSelectionCause::kFromOmnibox) ||
      last_tab_id == tab_id || url.spec() == kAndroidNativeNewTabPageURL) {
    return;
  }
  current_selection_->committed = true;
  tab_id_selection_map_[tab_id].emplace_back(*current_selection_);
  if (!tab_switcher_trigger_only_) {
    trigger_event_callback_.Run();
  }
}

void TabEventTrackerImpl::WillCloseTab(int tab_id) {
  closing_tabs_.insert(tab_id);
}

void TabEventTrackerImpl::TabClosureUndone(int tab_id) {
  closing_tabs_.erase(tab_id);
  // The cache could have suggestions without the reopened tab, which may need
  // to be included.
  invalidate_cache_callback_.Run();
}

void TabEventTrackerImpl::TabClosureCommitted(int tab_id) {
  closing_tabs_.erase(tab_id);
  tab_id_selection_map_.erase(tab_id);
}

void TabEventTrackerImpl::DidMoveTab(int tab_id,
                                     int new_index,
                                     int current_index) {}

void TabEventTrackerImpl::OnDidFinishNavigation(
    int tab_id,
    ui::PageTransition page_transition) {
  if (!ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_LINK) &&
      !ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_TYPED) &&
      !ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_AUTO_BOOKMARK) &&
      (page_transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) == 0) {
    return;
  }
  DCHECK(current_selection_.has_value());
  if (current_selection_->tab_id == tab_id && !current_selection_->committed) {
    current_selection_->committed = true;
    tab_id_selection_map_[tab_id].emplace_back(*current_selection_);
  }
  if (!tab_switcher_trigger_only_ && trigger_on_navigation_) {
    trigger_event_callback_.Run();
  }
}

void TabEventTrackerImpl::DidEnterTabSwitcher() {
  if (tab_switcher_trigger_only_) {
    trigger_event_callback_.Run();
  }
}

int TabEventTrackerImpl::GetSelectedCount(int tab_id) const {
  if (!closing_tabs_.contains(tab_id)) {
    if (auto it = tab_id_selection_map_.find(tab_id);
        it != tab_id_selection_map_.end()) {
      const std::vector<TabSelection>& selection_list = it->second;
      const auto time_now = base::Time::Now();
      return std::count_if(selection_list.begin(), selection_list.end(),
                           [=](const TabSelection selection) {
                             return time_now - selection.time <=
                                    kSelectionTimeWindow;
                           });
    }
  }
  return 0;
}

}  // namespace visited_url_ranking

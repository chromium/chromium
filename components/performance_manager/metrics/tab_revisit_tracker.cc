// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/tab_revisit_tracker.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"

namespace performance_manager {

namespace {

constexpr base::TimeDelta kMinTime = base::TimeDelta();
constexpr base::TimeDelta kMaxTime = base::Hours(48);

}  // namespace

TabRevisitTracker::TabRevisitTracker() = default;
TabRevisitTracker::~TabRevisitTracker() = default;

void TabRevisitTracker::RecordRevisitHistograms(
    const TabPageDecorator::TabHandle* tab_handle) {
  TabRevisitTracker::StateBundle state_bundle = tab_states_.at(tab_handle);
  CHECK(state_bundle.last_active_time.has_value());
  base::UmaHistogramCustomTimes(
      /*name=*/kTimeToRevisitHistogramName,
      /*time=*/base::TimeTicks::Now() - state_bundle.last_active_time.value(),
      /*minimum=*/kMinTime,
      /*maximum=*/kMaxTime,
      /*bucket_count=*/200);
}

void TabRevisitTracker::RecordCloseHistograms(
    const TabPageDecorator::TabHandle* tab_handle) {
  TabRevisitTracker::StateBundle state_bundle = tab_states_[tab_handle];
  CHECK(state_bundle.last_active_time.has_value());
  base::UmaHistogramCustomTimes(
      /*name=*/kTimeToCloseHistogramName,
      /*time=*/base::TimeTicks::Now() - state_bundle.last_active_time.value(),
      /*minimum=*/kMinTime,
      /*maximum=*/kMaxTime,
      /*bucket_count=*/200);
}

void TabRevisitTracker::OnPassedToGraph(Graph* graph) {
  TabPageDecorator* tab_page_decorator =
      graph->GetRegisteredObjectAs<TabPageDecorator>();
  CHECK(tab_page_decorator);
  tab_page_decorator->AddObserver(this);
}

void TabRevisitTracker::OnTakenFromGraph(Graph* graph) {
  TabPageDecorator* tab_page_decorator =
      graph->GetRegisteredObjectAs<TabPageDecorator>();
  if (tab_page_decorator) {
    tab_page_decorator->RemoveObserver(this);
  }
}

void TabRevisitTracker::OnTabAdded(TabPageDecorator::TabHandle* tab_handle) {
  PageLiveStateDecorator::Data* live_state_data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(
          tab_handle->page_node());
  CHECK(live_state_data);

  live_state_data->AddObserver(this);

  if (live_state_data->IsActiveTab()) {
    tab_states_[tab_handle].state = State::kActive;
    tab_states_[tab_handle].last_active_time = absl::nullopt;
  } else {
    tab_states_[tab_handle].state = State::kBackground;
    // Set the last active time to now, since it's used to measure time
    // spent in the background and this tab is already in the background.
    tab_states_[tab_handle].last_active_time = base::TimeTicks::Now();
  }
}

void TabRevisitTracker::OnTabAboutToBeDiscarded(
    const PageNode* old_page_node,
    TabPageDecorator::TabHandle* tab_handle) {
  PageLiveStateDecorator::Data* old_live_state_data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(old_page_node);
  CHECK(old_live_state_data);

  old_live_state_data->RemoveObserver(this);

  PageLiveStateDecorator::Data* new_live_state_data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(
          tab_handle->page_node());
  CHECK(new_live_state_data);

  new_live_state_data->AddObserver(this);

  tab_states_[tab_handle].state = State::kDiscarded;
}

void TabRevisitTracker::OnBeforeTabRemoved(
    TabPageDecorator::TabHandle* tab_handle) {
  PageLiveStateDecorator::Data* live_state_data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(
          tab_handle->page_node());
  CHECK(live_state_data);

  live_state_data->RemoveObserver(this);

  // Don't record the histograms if this is the active tab. We only care about
  // background tabs being closed.
  if (!live_state_data->IsActiveTab()) {
    RecordCloseHistograms(tab_handle);
  }

  tab_states_.erase(tab_handle);
}

void TabRevisitTracker::OnIsActiveTabChanged(const PageNode* page_node) {
  PageLiveStateDecorator::Data* live_state_data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node);
  CHECK(live_state_data);

  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(page_node);
  CHECK(tab_handle);

  if (live_state_data->IsActiveTab()) {
    CHECK_NE(tab_states_[tab_handle].state, State::kActive);
    tab_states_[tab_handle].state = State::kActive;
    RecordRevisitHistograms(tab_handle);
  } else {
    CHECK_NE(tab_states_[tab_handle].state, State::kBackground);
    tab_states_[tab_handle].state = State::kBackground;
    tab_states_[tab_handle].last_active_time = base::TimeTicks::Now();
  }
}

}  // namespace performance_manager

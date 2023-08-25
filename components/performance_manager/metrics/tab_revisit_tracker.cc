// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/tab_revisit_tracker.h"

#include <algorithm>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace performance_manager {

namespace {

constexpr base::TimeDelta kMinTime = base::TimeDelta();
constexpr base::TimeDelta kMaxTime = base::Hours(48);
constexpr int64_t kMaxNumRevisit = 20;
// Choosing a bucket spacing of 1.1 because it roughly matches the spacing of
// the 200 buckets, capped at 48 hours close/revisit histograms.
constexpr double kTimeBucketSpacing = 1.1;

int64_t GetLinearCappedBucket(int64_t sample, int64_t max) {
  return std::min(sample, max);
}

}  // namespace

TabRevisitTracker::TabRevisitTracker() = default;
TabRevisitTracker::~TabRevisitTracker() = default;

void TabRevisitTracker::RecordRevisitHistograms(
    const TabPageDecorator::TabHandle* tab_handle) {
  TabRevisitTracker::StateBundle state_bundle = tab_states_.at(tab_handle);
  CHECK(state_bundle.last_active_time.has_value());
  base::UmaHistogramCustomCounts(
      /*name=*/kTimeToRevisitHistogramName,
      /*sample=*/
      (base::TimeTicks::Now() - state_bundle.last_active_time.value())
          .InSeconds(),
      /*min=*/kMinTime.InSeconds(),
      /*exclusive_max=*/kMaxTime.InSeconds(),
      /*buckets=*/200);
}

void TabRevisitTracker::RecordCloseHistograms(
    const TabPageDecorator::TabHandle* tab_handle) {
  TabRevisitTracker::StateBundle state_bundle = tab_states_[tab_handle];
  CHECK(state_bundle.last_active_time.has_value());
  base::UmaHistogramCustomCounts(
      /*name=*/kTimeToCloseHistogramName,
      /*sample=*/
      (base::TimeTicks::Now() - state_bundle.last_active_time.value())
          .InSeconds(),
      /*min=*/kMinTime.InSeconds(),
      /*exclusive_max=*/kMaxTime.InSeconds(),
      /*buckets=*/200);
}

void TabRevisitTracker::RecordStateChangeUkm(
    const TabPageDecorator::TabHandle* tab_handle,
    State new_state) {
  ukm::builders::TabRevisitTracker_TabStateChange builder(
      tab_handle->page_node()->GetUkmSourceID());

  StateBundle& bundle = tab_states_.at(tab_handle);

  if (new_state == State::kActive) {
    ++bundle.num_revisits;
  }

  builder.SetPreviousState(StateToSample(bundle.state))
      .SetNewState(StateToSample(new_state))
      .SetNumTotalRevisits(
          GetLinearCappedBucket(bundle.num_revisits, kMaxNumRevisit))
      .SetTimeInPreviousState(ExponentiallyBucketedSeconds(
          base::TimeTicks::Now() - bundle.last_state_change_time));

  builder.Record(ukm::UkmRecorder::Get());

  bundle.state = new_state;
  bundle.last_state_change_time = base::TimeTicks::Now();
}

int64_t TabRevisitTracker::StateToSample(TabRevisitTracker::State state) {
  // The UKM doesn't report discarded tabs, instead treating them as in the
  // background.
  if (state == TabRevisitTracker::State::kDiscarded) {
    state = TabRevisitTracker::State::kBackground;
  }
  CHECK_LE(state, TabRevisitTracker::State::kClosed);
  return static_cast<int64_t>(state);
}

// static
int64_t TabRevisitTracker::ExponentiallyBucketedSeconds(base::TimeDelta time) {
  // Cap the reported time at 48 hours, effectively making the 48 hour bucket
  // the overflow bucket.
  int64_t seconds = std::min(time, kMaxTime).InSeconds();

  return ukm::GetExponentialBucketMin(seconds, kTimeBucketSpacing);
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
  tab_states_[tab_handle].last_state_change_time = base::TimeTicks::Now();
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
  // background tabs being closed in that histogram.
  if (!live_state_data->IsActiveTab()) {
    RecordCloseHistograms(tab_handle);
  }

  RecordStateChangeUkm(tab_handle, State::kClosed);

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
    RecordStateChangeUkm(tab_handle, State::kActive);
    tab_states_[tab_handle].state = State::kActive;
    RecordRevisitHistograms(tab_handle);
  } else {
    CHECK_NE(tab_states_[tab_handle].state, State::kBackground);
    tab_states_[tab_handle].last_active_time = base::TimeTicks::Now();
    RecordStateChangeUkm(tab_handle, State::kBackground);
    tab_states_[tab_handle].state = State::kBackground;
  }
}

}  // namespace performance_manager

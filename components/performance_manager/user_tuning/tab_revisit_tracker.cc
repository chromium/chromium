// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/user_tuning/tab_revisit_tracker.h"

#include <algorithm>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace performance_manager {

namespace {

constexpr base::TimeDelta kMinTime = base::TimeDelta();
constexpr base::TimeDelta kMaxTime = base::Hours(48);
// Choosing a bucket spacing of 1.1 because it roughly matches the spacing of
// the 200 buckets, capped at 48 hours close/revisit histograms.
constexpr double kTimeBucketSpacing = 1.1;

int64_t GetLinearCappedBucket(int64_t sample, int64_t max) {
  return std::min(sample, max);
}

int64_t StateToSample(TabRevisitTracker::State state) {
  // The UKM doesn't report discarded tabs, instead treating them as in the
  // background.
  if (state == TabRevisitTracker::State::kDiscarded) {
    state = TabRevisitTracker::State::kBackground;
  }
  CHECK_LE(state, TabRevisitTracker::State::kClosed);
  return static_cast<int64_t>(state);
}

void DoRecordUkm(TabRevisitTracker::StateBundle previous_state,
                 TabRevisitTracker::StateBundle new_state,
                 ukm::SourceId source_id) {
  CHECK_NE(ukm::kInvalidSourceId, source_id);

  ukm::builders::TabRevisitTracker_TabStateChange builder(source_id);

  builder.SetPreviousState(StateToSample(previous_state.state))
      .SetNewState(StateToSample(new_state.state))
      .SetNumTotalRevisits(GetLinearCappedBucket(
          new_state.num_revisits, TabRevisitTracker::kMaxNumRevisit))
      .SetTimeInPreviousState(TabRevisitTracker::ExponentiallyBucketedSeconds(
          base::TimeTicks::Now() - previous_state.last_state_change_time))
      .SetTotalTimeActive(TabRevisitTracker::ExponentiallyBucketedSeconds(
          new_state.total_time_active));

  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace

class TabRevisitTracker::UkmSourceIdReadyRecorder {
 public:
  UkmSourceIdReadyRecorder(StateBundle previous_state, StateBundle new_state)
      : previous_state_(previous_state), new_state_(new_state) {}

  void OnUkmSourceIdChanged(ukm::SourceId source_id) {
    CHECK_NE(source_id, ukm::kInvalidSourceId);
    DoRecordUkm(previous_state_, new_state_, source_id);
  }

  StateBundle previous_state_;
  StateBundle new_state_;
};

TabRevisitTracker::TabRevisitTracker() = default;
TabRevisitTracker::~TabRevisitTracker() = default;

TabRevisitTracker::StateBundle TabRevisitTracker::GetStateForTabHandle(
    const TabPageDecorator::TabHandle* tab_handle) {
  auto it = tab_states_.find(tab_handle);
  CHECK(it != tab_states_.end());
  return it->second;
}

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

void TabRevisitTracker::RecordStateChangeUkmAndUpdateStateBundle(
    const TabPageDecorator::TabHandle* tab_handle,
    StateBundle new_state_bundle) {
  // This may return the "invalid UKM source ID" for discarded
  // tabs, because their source ID is set on navigation, which happens later
  // than the `PageNode`'s activation. If this is the invalid source ID, an
  // observer is set up on the source ID property and the UKM event
  // is reported after when it becomes available.
  ukm::builders::TabRevisitTracker_TabStateChange builder(
      tab_handle->page_node()->GetUkmSourceID());

  auto it = tab_states_.find(tab_handle);
  CHECK(it != tab_states_.end());

  auto source_id = tab_handle->page_node()->GetUkmSourceID();
  if (source_id == ukm::kInvalidSourceId) {
    pending_ukm_records_.emplace(tab_handle,
                                 std::make_unique<UkmSourceIdReadyRecorder>(
                                     it->second, new_state_bundle));
  } else {
    DoRecordUkm(it->second, new_state_bundle, source_id);
  }

  it->second = new_state_bundle;
}

TabRevisitTracker::StateBundle TabRevisitTracker::CreateUpdatedStateBundle(
    const TabPageDecorator::TabHandle* tab_handle,
    State new_state) const {
  StateBundle new_bundle = tab_states_.at(tab_handle);

  if (new_state == State::kActive) {
    ++new_bundle.num_revisits;
  }

  const base::TimeTicks now = base::TimeTicks::Now();

  // Add the time spent in the last state to total_active_time and update
  // last_active_time if the tab was active before this transition.
  if (new_bundle.state == State::kActive) {
    new_bundle.last_active_time = now;
    new_bundle.total_time_active += (now - new_bundle.last_state_change_time);
  }

  new_bundle.state = new_state;
  new_bundle.last_state_change_time = now;

  return new_bundle;
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

  graph->AddPageNodeObserver(this);
}

void TabRevisitTracker::OnTakenFromGraph(Graph* graph) {
  TabPageDecorator* tab_page_decorator =
      graph->GetRegisteredObjectAs<TabPageDecorator>();
  if (tab_page_decorator) {
    tab_page_decorator->RemoveObserver(this);
  }

  graph->RemovePageNodeObserver(this);
}

void TabRevisitTracker::OnTabAdded(TabPageDecorator::TabHandle* tab_handle) {
  PageLiveStateDecorator::Data* live_state_data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(
          tab_handle->page_node());
  CHECK(live_state_data);

  live_state_data->AddObserver(this);

  if (live_state_data->IsActiveTab()) {
    tab_states_[tab_handle].state = State::kActive;
    tab_states_[tab_handle].last_active_time = std::nullopt;
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

  RecordStateChangeUkmAndUpdateStateBundle(
      tab_handle, CreateUpdatedStateBundle(tab_handle, State::kClosed));
  // No need to update anything in the State Bundle here since the page is going
  // away.

  tab_states_.erase(tab_handle);
  // If there was a pending record for this tab that never materialized, remove
  // it from the map.
  pending_ukm_records_.erase(tab_handle);
}

void TabRevisitTracker::OnIsActiveTabChanged(const PageNode* page_node) {
  PageLiveStateDecorator::Data* live_state_data =
      PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node);
  CHECK(live_state_data);

  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(page_node);
  CHECK(tab_handle);

  State new_state =
      live_state_data->IsActiveTab() ? State::kActive : State::kBackground;
  CHECK_NE(tab_states_[tab_handle].state, new_state);
  RecordStateChangeUkmAndUpdateStateBundle(
      tab_handle, CreateUpdatedStateBundle(tab_handle, new_state));

  if (live_state_data->IsActiveTab()) {
    RecordRevisitHistograms(tab_handle);
  }
}

void TabRevisitTracker::OnUkmSourceIdChanged(const PageNode* page_node) {
  TabPageDecorator::TabHandle* tab_handle =
      TabPageDecorator::FromPageNode(page_node);
  if (tab_handle) {
    auto it = pending_ukm_records_.find(tab_handle);
    if (it != pending_ukm_records_.end()) {
      it->second->OnUkmSourceIdChanged(page_node->GetUkmSourceID());

      // Once the recorder has done its job, it can be deleted.
      pending_ukm_records_.erase(tab_handle);
    }
  }
}

}  // namespace performance_manager

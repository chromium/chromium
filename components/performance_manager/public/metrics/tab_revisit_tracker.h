// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_TAB_REVISIT_TRACKER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_TAB_REVISIT_TRACKER_H_

#include <map>

#include "base/time/time.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/decorators/tab_connectedness_decorator.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {

// A GraphOwned object that tracks tab transitions to/from
// active/background/closed/discarded states and records timing information
// about these states.
class TabRevisitTracker : public GraphOwned,
                          public TabPageObserver,
                          public PageLiveStateObserverDefaultImpl,
                          public TabConnectednessDecorator::Observer,
                          public PageNode::ObserverDefaultImpl {
 public:
  static constexpr char kTimeToRevisitHistogramName[] =
      "PerformanceManager.TabRevisitTracker.TimeToRevisit2";
  static constexpr char kTimeToCloseHistogramName[] =
      "PerformanceManager.TabRevisitTracker.TimeToClose2";

  TabRevisitTracker();
  ~TabRevisitTracker() override;

  static int64_t ExponentiallyBucketedSeconds(base::TimeDelta time);

  enum class State {
    // The order of the leading elements must match the one in enums.xml
    // `TabRevisitTracker.TabState`.
    kActive,
    kBackground,
    kClosed,
    // The following entries aren't present in enums.xml but they are used for
    // internal tracking
    kDiscarded,
  };

  struct StateBundle {
    State state;
    absl::optional<base::TimeTicks> last_active_time;
    base::TimeDelta total_time_active;
    base::TimeTicks last_state_change_time;
    int64_t num_revisits;
    // This tab's connectedness score to the previously active tab when it last
    // became active. Stored as an int64_t because that's what is supported in
    // histograms, so the connectedness score (expressed as a foat in the range
    // [0, 1]) is remapped as an int in the range [0, 1000]. Can possibly be
    // `nullopt` if the tab was never connected to the active tab.
    absl::optional<int64_t> connectedness_to_last_switch_active_tab;
  };

 private:
  friend class TabRevisitTrackerTest;

  class UkmSourceIdReadyRecorder;

  void RecordRevisitHistograms(const TabPageDecorator::TabHandle* tab_handle);
  void RecordCloseHistograms(const TabPageDecorator::TabHandle* tab_handle);
  void RecordStateChangeUkmAndUpdateStateBundle(
      const TabPageDecorator::TabHandle* tab_handle,
      StateBundle new_state_bundle);

  StateBundle CreateUpdatedStateBundle(
      const TabPageDecorator::TabHandle* tab_handle,
      State new_state) const;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // TabPageObserver:
  void OnTabAdded(TabPageDecorator::TabHandle* tab_handle) override;
  void OnTabAboutToBeDiscarded(
      const PageNode* old_page_node,
      TabPageDecorator::TabHandle* tab_handle) override;
  void OnBeforeTabRemoved(TabPageDecorator::TabHandle* tab_handle) override;

  // PageLiveStateObserverDefaultImpl:
  void OnIsActiveTabChanged(const PageNode* page_node) override;

  // TabConnectednessDecorator::Observer:
  void OnBeforeTabSwitch(TabPageDecorator::TabHandle* source,
                         TabPageDecorator::TabHandle* destination) override;

  // PageNode::ObserverDefaultImpl:
  void OnUkmSourceIdChanged(const PageNode* page_node) override;

  std::map<const TabPageDecorator::TabHandle*, StateBundle> tab_states_;
  std::map<const TabPageDecorator::TabHandle*,
           std::unique_ptr<UkmSourceIdReadyRecorder>>
      pending_ukm_records_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_TAB_REVISIT_TRACKER_H_

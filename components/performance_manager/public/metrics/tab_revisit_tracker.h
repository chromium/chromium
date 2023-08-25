// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_TAB_REVISIT_TRACKER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_TAB_REVISIT_TRACKER_H_

#include <map>

#include "base/time/time.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {

// A GraphOwned object that tracks tab transitions to/from
// active/background/closed/discarded states and records timing information
// about these states.
class TabRevisitTracker : public GraphOwned,
                          public TabPageObserver,
                          public PageLiveStateObserverDefaultImpl {
 public:
  static constexpr char kTimeToRevisitHistogramName[] =
      "PerformanceManager.TabRevisitTracker.TimeToRevisit2";
  static constexpr char kTimeToCloseHistogramName[] =
      "PerformanceManager.TabRevisitTracker.TimeToClose2";

  TabRevisitTracker();
  ~TabRevisitTracker() override;

 private:
  friend class TabRevisitTrackerTest;

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
    base::TimeTicks last_state_change_time;
    int64_t num_revisits;
  };

  void RecordRevisitHistograms(const TabPageDecorator::TabHandle* tab_handle);
  void RecordCloseHistograms(const TabPageDecorator::TabHandle* tab_handle);
  void RecordStateChangeUkm(const TabPageDecorator::TabHandle* tab_handle,
                            State new_state);

  int64_t StateToSample(TabRevisitTracker::State state);
  static int64_t ExponentiallyBucketedSeconds(base::TimeDelta time);

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

  std::map<const TabPageDecorator::TabHandle*, StateBundle> tab_states_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_TAB_REVISIT_TRACKER_H_

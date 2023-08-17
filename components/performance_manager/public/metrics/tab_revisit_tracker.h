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
                          public PageLiveStateObserver {
 public:
  static constexpr char kTimeToRevisitHistogramName[] =
      "PerformanceManager.TabRevisitTracker.TimeToRevisit";
  static constexpr char kTimeToCloseHistogramName[] =
      "PerformanceManager.TabRevisitTracker.TimeToClose";

  TabRevisitTracker();
  ~TabRevisitTracker() override;

 private:
  enum class State {
    kActive,
    kBackground,
    kDiscarded,
  };

  struct StateBundle {
    State state;
    absl::optional<base::TimeTicks> last_active_time;
  };

  void RecordRevisitHistograms(const TabPageDecorator::TabHandle* tab_handle);
  void RecordCloseHistograms(const TabPageDecorator::TabHandle* tab_handle);

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // TabPageObserver:
  void OnTabAdded(TabPageDecorator::TabHandle* tab_handle) override;
  void OnTabAboutToBeDiscarded(
      const PageNode* old_page_node,
      TabPageDecorator::TabHandle* tab_handle) override;
  void OnBeforeTabRemoved(TabPageDecorator::TabHandle* tab_handle) override;

  // PageLiveStateObserver:
  void OnIsActiveTabChanged(const PageNode* page_node) override;
  // We only care about `OnIsActiveTabChanged` but these are all pure virtual so
  // no-op here.
  void OnIsConnectedToUSBDeviceChanged(const PageNode* page_node) override {}
  void OnIsConnectedToBluetoothDeviceChanged(
      const PageNode* page_node) override {}
  void OnIsCapturingVideoChanged(const PageNode* page_node) override {}
  void OnIsCapturingAudioChanged(const PageNode* page_node) override {}
  void OnIsBeingMirroredChanged(const PageNode* page_node) override {}
  void OnIsCapturingWindowChanged(const PageNode* page_node) override {}
  void OnIsCapturingDisplayChanged(const PageNode* page_node) override {}
  void OnIsAutoDiscardableChanged(const PageNode* page_node) override {}
  void OnWasDiscardedChanged(const PageNode* page_node) override {}
  void OnIsPinnedTabChanged(const PageNode* page_node) override {}
  void OnContentSettingsChanged(const PageNode* page_node) override {}
  void OnIsDevToolsOpenChanged(const PageNode* page_node) override {}

  std::map<const TabPageDecorator::TabHandle*, StateBundle> tab_states_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_TAB_REVISIT_TRACKER_H_

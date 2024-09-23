// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_

#include <map>
#include <memory>
#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/freezing/cannot_freeze_reason.h"
#include "components/performance_manager/freezing/freezer.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "content/public/browser/browsing_instance_id.h"

namespace performance_manager {

// Freezes sets of connected pages when no page in the set is opted-out and:
// - All pages have at least one freezing vote, or,
// - A group of same-origin and same-browsing-instance frames/workers associated
//   with the set of connected pages used a lot of CPU in the background and
//   Battery Saver is active.
//
// Pages are connected if they host frames from the same browsing instance. For
// example:
// - Page A hosts frames from browsing instance 1
// - Page B hosts frames from browsing instances 1 and 2
// - Page C hosts frames from browsing instance 2
// - Page D hosts frames from browsing instance 3
// The sets of connected pages are {A, B, C} and {D}.
//
// A page is opted-out from freezing when it is:
//   - Visible;
//   - Audible;
//   - Recently audible;
//   - Holding at least one WebLock;
//   - Holding at least one IndexedDB lock;
//   - Connected to a USB device;
//   - Connected to a bluetooth device;
//   - Capturing video;
//   - Capturing audio;
//   - Mirrored;
//   - Capturing window;
//   - Capturing display;
class FreezingPolicy : public PageNode::ObserverDefaultImpl,
                       public FrameNode::ObserverDefaultImpl,
                       public PageLiveStateObserverDefaultImpl,
                       public resource_attribution::QueryResultObserver,
                       public GraphOwnedAndRegistered<FreezingPolicy>,
                       public NodeDataDescriberDefaultImpl {
 public:
  FreezingPolicy();
  FreezingPolicy(const FreezingPolicy&) = delete;
  FreezingPolicy& operator=(const FreezingPolicy&) = delete;
  ~FreezingPolicy() override;

  void SetFreezerForTesting(std::unique_ptr<Freezer> freezer) {
    freezer_ = std::move(freezer);
  }

  // Invoked freezing on battery saver is enabled or disabled.
  void ToggleFreezingOnBatterySaverMode(bool is_enabled);

  // Add or remove a freezing vote for `page_node`. A browsing instance is
  // frozen if all its pages have a freezing vote and none have a
  // `CannotFreezeReason`.
  void AddFreezeVote(PageNode* page_node);
  void RemoveFreezeVote(PageNode* page_node);

 private:
  // State of a browsing instance.
  struct BrowsingInstanceState {
    BrowsingInstanceState();
    ~BrowsingInstanceState();

    // Pages that have frames in this browsing instance (typically only 1 page,
    // but may contain an unbounded amount of pages connected via opener
    // relationship).
    base::flat_set<const PageNode*> pages;
    // Whether a group of same-origin frames/workers associated with this
    // browsing instance used a lot of CPU in background.
    bool cpu_intensive_in_background = false;
    // Whether a page associated with this browsing instance had a
    // `CannotFreezeReason` at any time since the last CPU measurement.
    bool had_cannot_freeze_reason_since_last_cpu_measurement = false;
  };

  // Returns browsing instance id(s) for `page`.
  base::flat_set<content::BrowsingInstanceId> GetBrowsingInstances(
      const PageNode* page) const;

  // Update frozen state for all pages connected to `page`. Connected pages
  // (including `page_node`) are added to `connected_pages_out` if not nullptr.
  void UpdateFrozenState(
      const PageNode* page_node,
      base::flat_set<raw_ptr<const PageNode>>* connected_pages_out = nullptr);

  // Helper to add or remove a `CannotFreezeReason` for `page_node`.
  void OnCannotFreezeReasonChange(const PageNode* page_node,
                                  bool add,
                                  CannotFreezeReason reason);

  // Returns true iff a page associated with `browsing_instance_state` has a
  // `CannotFreezeReason`.
  static bool HasCannotFreezeReason(
      const BrowsingInstanceState& browsing_instance_state);

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnIsAudibleChanged(const PageNode* page_node) override;
  void OnPageIsHoldingWebLockChanged(const PageNode* page_node) override;
  void OnPageIsHoldingIndexedDBLockChanged(const PageNode* page_node) override;
  void OnPageUsesWebRTCChanged(const PageNode* page_node) override;
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnIsAudibleChanged(const FrameNode* frame_node) override;

  // PageLiveStateObserverDefaultImpl:
  void OnIsConnectedToUSBDeviceChanged(const PageNode* page_node) override;
  void OnIsConnectedToBluetoothDeviceChanged(
      const PageNode* page_node) override;
  void OnIsConnectedToHidDeviceChanged(const PageNode* page_node) override;
  void OnIsConnectedToSerialPortChanged(const PageNode* page_node) override;
  void OnIsCapturingVideoChanged(const PageNode* page_node) override;
  void OnIsCapturingAudioChanged(const PageNode* page_node) override;
  void OnIsBeingMirroredChanged(const PageNode* page_node) override;
  void OnIsCapturingWindowChanged(const PageNode* page_node) override;
  void OnIsCapturingDisplayChanged(const PageNode* page_node) override;

  // NodeDataDescriber:
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;

  // resource_attribution::QueryResultObserver:
  void OnResourceUsageUpdated(
      const resource_attribution::QueryResultMap& results) override;

  // Used to freeze pages.
  std::unique_ptr<Freezer> freezer_;

  // State of each browsing instance.
  std::map<content::BrowsingInstanceId, BrowsingInstanceState>
      browsing_instances_;

  // Whether Battery Saver is currently active.
  bool is_battery_saver_active_ = false;

  // Measures cumulative CPU usage per group of frames/workers that belong to
  // the same [browsing instance, origin]. Engaged when the
  // "CPUMeasurementInFreezingPolicy" feature is enabled.
  std::optional<resource_attribution::ScopedResourceUsageQuery>
      cpu_usage_query_;

  // Manages observation of `cpu_usage_query_` by `this`.
  resource_attribution::ScopedQueryObservation cpu_usage_query_observation_{
      this};

  // Calculates the proportion of CPU used by a group of frames/workers that
  // belong to the same [browsing instance, origin] over an interval, based on
  // cumulative measurements from `cpu_usage_query_`.
  resource_attribution::CPUProportionTracker cpu_proportion_tracker_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/performance_manager/freezing/cannot_freeze_reason.h"
#include "components/performance_manager/freezing/freezer.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "content/public/browser/browsing_instance_id.h"

namespace performance_manager {

// Freezes browsing instances when all their pages have a freezing vote and
// there is no reason not to freeze it.
//
// Reasons not to freeze a browsing instance:
//   - Visible;
//   - Audible;
//   - Recently audible;
//   - Holding at least one WebLock.
//   - Holding at least one IndexedDB lock;
//   - Connected to a USB device;
//   - Connected to a bluetooth device;
//   - Capturing video;
//   - Capturing audio;
//   - Mirrored;
//   - Capturing window;
//   - Capturing display;
class FreezingPolicy : public GraphObserver,
                       public GraphOwnedDefaultImpl,
                       public GraphRegisteredImpl<FreezingPolicy>,
                       public PageNode::ObserverDefaultImpl,
                       public FrameNode::ObserverDefaultImpl,
                       public PageLiveStateObserverDefaultImpl,
                       public NodeDataDescriberDefaultImpl {
 public:
  FreezingPolicy();
  FreezingPolicy(const FreezingPolicy&) = delete;
  FreezingPolicy& operator=(const FreezingPolicy&) = delete;
  ~FreezingPolicy() override;

  void SetFreezerForTesting(std::unique_ptr<Freezer> freezer) {
    freezer_ = std::move(freezer);
  }

  // Add or remove a freezing vote for `page_node`. A browsing instance is
  // frozen if all its pages have a freezing vote and none have a
  // `CannotFreezeReason`.
  void AddFreezeVote(PageNode* page_node);
  void RemoveFreezeVote(PageNode* page_node);

  static constexpr base::TimeDelta kAudioProtectionTime = base::Minutes(1);

 private:
  // State of a browsing instance.
  struct BrowsingInstanceState {
    BrowsingInstanceState();
    ~BrowsingInstanceState();

    // Pages that belong to this browsing instance (typically only 1 page, but
    // may contain an unbounded amount of pages connected via opener
    // relationship).
    base::flat_set<const PageNode*> pages;
    // Whether pages in the browsing instance are currently frozen.
    bool frozen = false;
  };

  // Returns browsing instance id(s) for `page`.
  base::flat_set<content::BrowsingInstanceId> GetBrowsingInstances(
      const PageNode* page) const;

  // Updates frozen state for `page_node`'s browsing instance(s).
  void UpdateFrozenState(const PageNode* page_node);

  // Helper to add or remove a `CannotFreezeReason` for `page_node`.
  void OnCannotFreezeReasonChange(const PageNode* page_node,
                                  bool add,
                                  CannotFreezeReason reason);

  // GraphObserver implementation:
  void OnBeforeGraphDestroyed(Graph* graph) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnIsAudibleChanged(const PageNode* page_node) override;
  void OnPageIsHoldingWebLockChanged(const PageNode* page_node) override;
  void OnPageIsHoldingIndexedDBLockChanged(const PageNode* page_node) override;
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
  void OnIsCapturingVideoChanged(const PageNode* page_node) override;
  void OnIsCapturingAudioChanged(const PageNode* page_node) override;
  void OnIsBeingMirroredChanged(const PageNode* page_node) override;
  void OnIsCapturingWindowChanged(const PageNode* page_node) override;
  void OnIsCapturingDisplayChanged(const PageNode* page_node) override;

  // NodeDataDescriber:
  base::Value::Dict DescribePageNodeData(const PageNode* node) const override;

  // Used to freeze pages.
  std::unique_ptr<Freezer> freezer_;

  // State of each browsing instance.
  std::map<content::BrowsingInstanceId, BrowsingInstanceState>
      browsing_instances_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_

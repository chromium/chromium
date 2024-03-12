// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_

#include <array>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager {

class Freezer;

// A simple freezing policy that attempts to freeze pages when their associated
// freezing vote is positive.
//
// Tabs in one of the following states won't be frozen:
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
//
// Note that visible tabs can't be frozen and tabs that becomes visible are
// automatically unfrozen, there's no need to track this feature here.
class FreezingPolicy : public GraphObserver,
                       public GraphOwnedDefaultImpl,
                       public PageNode::ObserverDefaultImpl,
                       public PageLiveStateObserverDefaultImpl {
 public:
  FreezingPolicy();
  FreezingPolicy(const FreezingPolicy&) = delete;
  FreezingPolicy(FreezingPolicy&&) = delete;
  FreezingPolicy& operator=(const FreezingPolicy&) = delete;
  FreezingPolicy& operator=(FreezingPolicy&&) = delete;
  ~FreezingPolicy() override;

  void SetFreezerForTesting(std::unique_ptr<Freezer> freezer) {
    freezer_ = std::move(freezer);
  }

  static constexpr base::TimeDelta kAudioProtectionTime = base::Minutes(1);

 protected:
  // List of states that prevent a tab from being frozen.
  enum CannotFreezeReason {
    kAudible = 0,
    kRecentlyAudible,
    kHoldingWebLock,
    kHoldingIndexedDBLock,
    kConnectedToUsbDevice,
    kConnectedToBluetoothDevice,
    kCapturingVideo,
    kCapturingAudio,
    kBeingMirrored,
    kCapturingWindow,
    kCapturingDisplay,
    kCount,
  };

  // Helper function to convert a |CannotFreezeReason| to a string.
  static const char* CannotFreezeReasonToString(CannotFreezeReason reason);

 private:
  // GraphObserver implementation:
  void OnBeforeGraphDestroyed(Graph* graph) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsAudibleChanged(const PageNode* page_node) override;
  void OnPageIsHoldingWebLockChanged(const PageNode* page_node) override;
  void OnPageIsHoldingIndexedDBLockChanged(const PageNode* page_node) override;
  void OnFreezingVoteChanged(
      const PageNode* page_node,
      std::optional<performance_manager::freezing::FreezingVote> previous_vote)
      override;
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;

  // PageLiveStateObserverDefaultImpl:
  void OnIsConnectedToUSBDeviceChanged(const PageNode* page_node) override;
  void OnIsConnectedToBluetoothDeviceChanged(
      const PageNode* page_node) override;
  void OnIsCapturingVideoChanged(const PageNode* page_node) override;
  void OnIsCapturingAudioChanged(const PageNode* page_node) override;
  void OnIsBeingMirroredChanged(const PageNode* page_node) override;
  void OnIsCapturingWindowChanged(const PageNode* page_node) override;
  void OnIsCapturingDisplayChanged(const PageNode* page_node) override;

  // Helper function that either calls SubmitNegativeVote() or
  // InvalidateNegativeVote() when the value of a property changes.
  void OnPropertyChanged(const PageNode* page_node,
                         bool submit_vote,
                         CannotFreezeReason reason);

  // Submits or invalidates a negative freezing vote for |page_node| for
  // |reason|. There can only be one vote associated with this reason.
  void SubmitNegativeFreezingVote(const PageNode* page_node,
                                  CannotFreezeReason reason);
  void InvalidateNegativeFreezingVote(const PageNode* page_node,
                                      CannotFreezeReason reason);

  // Holds one voting channel per CannotFreezeReason.
  std::array<freezing::FreezingVotingChannel, CannotFreezeReason::kCount>
      voting_channels_;

  // Map that associates the PageNodes that have recently been audible with a
  // timer used to clear the negative freezing vote used to protect these pages
  // from freezing.
  base::flat_map<const PageNode*, std::unique_ptr<base::OneShotTimer>>
      page_nodes_recently_audible_;

  // The page node being removed, used to avoid freezing a page node while it's
  // being removed.
  raw_ptr<const PageNode> page_node_being_removed_ = nullptr;

  // The freezing mechanism used to do the actual freezing.
  std::unique_ptr<Freezer> freezer_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_

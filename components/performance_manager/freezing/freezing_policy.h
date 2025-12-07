// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>

#include "base/byte_count.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/freezing/freezer.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/freezing/cannot_freeze_reason.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "content/public/browser/browsing_instance_id.h"
#include "url/origin.h"

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
// The `CannotFreezeReason` enum lists conditions that opt-out a page from
// freezing.
class FreezingPolicy : public PageNodeObserver,
                       public FrameNodeObserver,
                       public PageLiveStateObserver,
                       public resource_attribution::QueryResultObserver,
                       public GraphOwnedAndRegistered<FreezingPolicy>,
                       public NodeDataDescriberDefaultImpl {
 public:
  enum class FreezingType {
    // Freezing via the voting system exposed to embedders.
    kVoting,
    // Freezing of CPU-intensive background tabs when Battery Saver is active.
    kBatterySaver,
    // Freezing of tabs which aren't in the set of most recently used, to
    // scale to infinite tabs.
    kInfiniteTabs,
  };
  using FreezingTypeSet = base::
      EnumSet<FreezingType, FreezingType::kVoting, FreezingType::kInfiniteTabs>;

  explicit FreezingPolicy(
      std::unique_ptr<freezing::Discarder> discarder,
      std::unique_ptr<freezing::OptOutChecker> opt_out_checker = nullptr);
  ~FreezingPolicy() override;

  FreezingPolicy(const FreezingPolicy&) = delete;
  FreezingPolicy& operator=(const FreezingPolicy&) = delete;

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

  // Returns details about whether a page can be frozen.
  freezing::CanFreezeDetails GetCanFreezeDetails(const PageNode* page_node);

  // Returns a set of `CannotFreezeReason`s applicable to `freezing_type`.
  static freezing::CannotFreezeReasonSet CannotFreezeReasonsForType(
      FreezingPolicy::FreezingType type);

 private:
  FRIEND_TEST_ALL_PREFIXES(FreezingPolicyBatterySaverTest,
                           RecordFreezingEligibilityUKMForPageStatic);

  class CanFreezePerTypeTracker;

  // Freezing related state for a page.
  struct PageFreezingState;

  // Freezing related state for a browsing instance.
  struct BrowsingInstanceState {
    BrowsingInstanceState();
    ~BrowsingInstanceState();

    // Returns true if all pages in this browsing instance are frozen.
    bool AllPagesFrozen() const;

    // Pages that have frames in this browsing instance (typically only 1 page,
    // but may contain an unbounded amount of pages connected via opener
    // relationship).
    base::flat_set<const PageNode*> pages;
    // Highest CPU measurement for a group of same-origin frames/workers
    // associated with this browsing instance, over the last measurement period.
    // (1.0 = 100% of 1 core)
    std::optional<double> highest_cpu_current_interval;
    // Highest CPU measurement for a group of same-origin frames/workers
    // associated within this browsing instance, over any past measurement
    // period during which no `CannotFreezeReason` associated with
    // `FreezingType::kBatterySaver` was applicable. (1.0 = 100% of 1 core)
    double highest_cpu_without_battery_saver_cannot_freeze = 0.0;
    // `CannotFreezeReason`s applicable to this browsing instance at any point
    // since the last CPU measurement.
    freezing::CannotFreezeReasonSet
        cannot_freeze_reasons_since_last_cpu_measurement;
    // First per-origin Private Memory Footprint measurement taken after this
    // browsing instance became frozen. Empty if not all pages in this browsing
    // instance are frozen.
    base::flat_map<url::Origin, base::ByteCount> per_origin_pmf_after_freezing;
  };

  // Returns pages connected to `page`, including `page` itself. See
  // meta-comment above this class for a definition of "connected".
  base::flat_set<raw_ptr<const PageNode>> GetConnectedPages(
      const PageNode* page);

  // Returns browsing instance id(s) for `page`.
  base::flat_set<content::BrowsingInstanceId> GetBrowsingInstances(
      const PageNode* page) const;

  // Returns the `PageFreezingState` for `page`, creating it if necessary.
  PageFreezingState& GetFreezingState(const PageNode* page_node) const;

  // Update frozen state for all pages connected to `page`. Connected pages
  // (including `page_node`) are added to `connected_pages_out` if not nullptr.
  void UpdateFrozenState(
      const PageNode* page_node,
      base::TimeTicks now = base::TimeTicks::Now(),
      base::flat_set<raw_ptr<const PageNode>>* connected_pages_out = nullptr);

  // Helper to add or remove a `CannotFreezeReason` for `page_node`.
  void OnCannotFreezeReasonChange(const PageNode* page_node,
                                  bool add,
                                  freezing::CannotFreezeReason reason);

  // Returns the union of `CannotFreezeReason`s applicable to pages associated
  // with `browsing_instance_state`.
  freezing::CannotFreezeReasonSet GetCannotFreezeReasons(
      const BrowsingInstanceState& browsing_instance_state);

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnTypeChanged(const PageNode* page_node,
                     PageType previous_type) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnIsAudibleChanged(const PageNode* page_node) override;
  void OnPageLifecycleStateChanged(const PageNode* page_node) override;
  void OnPageHasFreezingOriginTrialOptOutChanged(
      const PageNode* page_node) override;
  void OnPageIsHoldingWebLockChanged(const PageNode* page_node) override;
  void OnPageIsHoldingBlockingIndexedDBLockChanged(
      const PageNode* page_node) override;
  void OnPageUsesWebRTCChanged(const PageNode* page_node) override;
  void OnPageNotificationPermissionStatusChange(
      const PageNode* page_node,
      std::optional<blink::mojom::PermissionStatus> previous_status) override;
  void OnMainFrameUrlChanged(const PageNode* page_node) override;
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnFrameNodeRemoved(
      const FrameNode* frame_node,
      const FrameNode* previous_parent_frame_node,
      const PageNode* previous_page_node,
      const ProcessNode* previous_process_node,
      const FrameNode* previous_parent_or_outer_document_or_embedder) override;
  void OnIsAudibleChanged(const FrameNode* frame_node) override;

  // PageLiveStateObserver:
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

  // Discards browsing instances for which memory usage has significantly
  // increased since they were frozen upon receiving a memory measurement.
  void DiscardFrozenPagesWithGrowingMemoryOnMemoryMeasurement(
      const resource_attribution::QueryResultMap& results);

  // Updates the frozen state of all browsing instances upon receiving a CPU
  // measurement.
  void UpdateFrozenStateOnCPUMeasurement(
      const resource_attribution::QueryResultMap& results);

  // Invoked by the OptOutChecker when the opt-out policy for
  // `browser_context_id` changes.
  void OnOptOutPolicyChanged(std::string_view browser_context_id);

  // Removes the last page from the most recently used list if needed, to keep
  // its size below the limit.
  void MaybePopFromMostRecentlyUsedList();

  // Checks that the size of the most recently used list respects the limit.
  void CheckMostRecentlyUsedListSize();

  // Starts a timer to manage periodic unfreezing of a tab frozen for
  // `FreezingContext::kInfiniteTabs`. The timer is scheduled to invoke
  // OnPeriodicUnfreezeTimer() at the next time when the tab must be unfrozen or
  // re-frozen.
  void StartPeriodicUnfreezeTimer(const PageNode* page_node,
                                  base::TimeTicks now);

  // Method invoked when when it's time to unfreeze or re-freeze a tab frozen
  // for `FreezingContext::kInfiniteTabs`.
  void OnPeriodicUnfreezeTimer(const PageNode* page);

  // Records freezing eligibility UKM for all pages.
  void RecordFreezingEligibilityUKM();

  // Records freezing eligibility UKM for a page. Virtual for testing.
  virtual void RecordFreezingEligibilityUKMForPage(
      ukm::SourceId source_id,
      double highest_cpu_current_interval,
      double highest_cpu_without_battery_saver_cannot_freeze,
      freezing::CannotFreezeReasonSet battery_saver_cannot_freeze_reasons);

  // Records freezing eligibility UKM for a page. Static implementation.
  //
  // Note: The virtual method RecordFreezingEligibilityUKMForPage() and the
  // static method RecordFreezingEligibilityUKMForPageStatic() are separate to
  // facilitate testing the code that produces the inputs for the UKM event and
  // the code that records the UKM event based on these inputs separately.
  static void RecordFreezingEligibilityUKMForPageStatic(
      ukm::SourceId source_id,
      double highest_cpu_current_interval,
      double highest_cpu_without_battery_saver_cannot_freeze,
      freezing::CannotFreezeReasonSet battery_saver_cannot_freeze_reasons);

  // Returns a random periodic unfreeze phase. Can be overridden in test to
  // eliminate randomness.
  virtual base::TimeTicks GenerateRandomPeriodicUnfreezePhase() const;

  // Called when the memory pressure state of the system is updated. Triggers a
  // policy-wide re-evaluation of page freezing.
  void CheckMemoryPressureForFreezing();

  // Triggers a re-evaluation of the frozen state for all pages in the graph.
  void UpdateAllPagesFrozenState();

  // Used to freeze pages.
  std::unique_ptr<Freezer> freezer_;

  // Used to discard pages.
  std::unique_ptr<freezing::Discarder> discarder_;

  // Used to check whether pages are opted out of freezing by the embedder.
  std::unique_ptr<freezing::OptOutChecker> opt_out_checker_;

  // State of each browsing instance.
  std::map<content::BrowsingInstanceId, BrowsingInstanceState>
      browsing_instance_states_;

  // Whether Battery Saver is currently active.
  bool is_battery_saver_active_ = false;

  // Measures cumulative CPU usage per group of frames/workers that belong to
  // the same [browsing instance, origin]. Engaged when the
  // "CPUMeasurementInFreezingPolicy" feature is enabled.
  std::optional<resource_attribution::ScopedResourceUsageQuery>
      resource_usage_query_;

  // Manages observation of `resource_usage_query_` by `this`.
  resource_attribution::ScopedQueryObservation
      resource_usage_query_observation_{this};

  // Calculates the proportion of CPU used by a group of frames/workers that
  // belong to the same [browsing instance, origin] over an interval, based on
  // cumulative measurements from `resource_usage_query_`.
  resource_attribution::CPUProportionTracker cpu_proportion_tracker_;

  // Used to subsample the emission of UKM events.
  base::MetricsSubSampler metrics_subsampler_;

  // List of most recently used hidden tabs. A tab becomes the most recently
  // used when it transitions from visible to hidden, or when it's created in a
  // hidden state.
  std::deque<raw_ptr<const PageNode>> most_recently_used_;

  // Number of visible tabs.
  int num_visible_tabs_ = 0;

  // Timer to periodically check system memory.
  base::RepeatingTimer memory_check_timer_;

  // True if the system is considered to be under memory pressure by our
  // internal check.
  bool is_under_memory_pressure_ = false;

  base::WeakPtrFactory<FreezingPolicy> weak_factory_{this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_FREEZING_FREEZING_POLICY_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_policy.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/byte_count.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/freezing/cannot_freeze_reason.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/origin_in_browsing_instance_context.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using freezing::CannotFreezeReason;
using freezing::CannotFreezeReasonSet;
using resource_attribution::OriginInBrowsingInstanceContext;

constexpr base::TimeDelta kCPUMeasurementInterval = base::Minutes(1);

bool HasCannotFreezeReasonForType(
    const CannotFreezeReasonSet& cannot_freeze_reasons,
    FreezingPolicy::FreezingType type) {
  return !base::Intersection(cannot_freeze_reasons,
                             FreezingPolicy::CannotFreezeReasonsForType(type))
              .empty();
}

bool IsPageConnectedToUSBDevice(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsConnectedToUSBDevice();
}

bool IsPageConnectedToBluetoothDevice(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsConnectedToBluetoothDevice();
}

bool IsPageConnectedToHidDevice(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsConnectedToHidDevice();
}

bool IsPageConnectedToSerialPort(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsConnectedToSerialPort();
}

bool IsPageCapturingVideo(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsCapturingVideo();
}

bool IsPageCapturingAudio(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsCapturingAudio();
}

bool IsPageBeingMirrored(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsBeingMirrored();
}

bool IsPageCapturingWindow(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsCapturingWindow();
}

bool IsPageCapturingDisplay(const PageNode* page_node) {
  return PageLiveStateDecorator::Data::FromPageNode(page_node)
      ->IsCapturingDisplay();
}

}  // namespace

struct FreezingPolicy::PageFreezingState
    : public NodeAttachedDataImpl<PageFreezingState> {
  explicit PageFreezingState(const PageNodeImpl*) {}
  ~PageFreezingState() override = default;

  PageFreezingState(const PageFreezingState&) = delete;
  PageFreezingState& operator=(const PageFreezingState&) = delete;

  // Returns the start time (inclusive) and end time (exclusive) of the current
  // periodic unfreeze period, or of the next one if not currently in a periodic
  // unfreeze period. A periodic unfreeze period is a period during which a tab
  // frozen with `FreezingType::kInfiniteTabs` is temporarily unfrozen.
  // Concretely, if the page must be unfrozen from t=10 (incl) to t=12 (excl)
  // and from t=20 (incl) to t=22 (excl):
  //         If `now` is 10 -> returns [10, 12]
  //                     11 ->         [10, 12]
  //                     12 ->         [20, 22]
  //                     13 ->         [20, 22]
  //         etc.
  std::pair<base::TimeTicks, base::TimeTicks>
  GetCurrentOrNextUnfreezePeriodStart(base::TimeTicks now) const {
    const base::TimeTicks next_unfreeze_time = now.SnappedToNextTick(
        *periodic_unfreeze_phase,
        features::kInfiniteTabsFreezing_UnfreezeInterval.Get());
    const base::TimeTicks previous_unfreeze_time =
        next_unfreeze_time -
        features::kInfiniteTabsFreezing_UnfreezeInterval.Get();
    CHECK_LT(previous_unfreeze_time, now);

    if ((previous_unfreeze_time +
         features::kInfiniteTabsFreezing_UnfreezeDuration.Get()) > now) {
      return {previous_unfreeze_time,
              previous_unfreeze_time +
                  features::kInfiniteTabsFreezing_UnfreezeDuration.Get()};
    }

    return {next_unfreeze_time,
            next_unfreeze_time +
                features::kInfiniteTabsFreezing_UnfreezeDuration.Get()};
  }

  // Returns true if `now` is within a periodic unfreeze period for this page.
  bool IsInUnfreezePeriod(base::TimeTicks now) const {
    return GetCurrentOrNextUnfreezePeriodStart(now).first <= now;
  }

  // Returns the delay until the next start or end of a periodic unfreeze period
  // for this page. The return value is guaranteed to be greater than zero (if
  // there is a state change at time `now`, this returns the time of the next
  // state change).
  base::TimeDelta GetDelayUntilNextUnfreezeStateChange(
      base::TimeTicks now) const {
    auto [start_incl, end_excl] = GetCurrentOrNextUnfreezePeriodStart(now);
    if (start_incl > now) {
      return start_incl - now;
    }
    CHECK_GT(end_excl, now);
    return end_excl - now;
  }

  // Whether this page is frozen.
  bool frozen = false;

  // Number of votes to freeze the page.
  int num_freeze_votes = 0;

  // Phase for periodic unfreezing. Use a random value so that different tabs
  // are unfrozen at different tabs as much as possible, but also cannot learn
  // anything about other unrelated tabs.
  std::optional<base::TimeTicks> periodic_unfreeze_phase;

  // Reasons not to freeze the page.
  CannotFreezeReasonSet cannot_freeze_reasons;

  // Timer to remove `CannotFreezeReason::kRecentlyVisible`.
  base::OneShotTimer recently_visible_timer;

  // Timer to remove `CannotFreezeReason::kRecentlyAudible`.
  base::OneShotTimer recently_audible_timer;

  // Timer for periodic unfreezing.
  base::OneShotTimer periodic_unfreeze_timer;
};

class FreezingPolicy::CanFreezePerTypeTracker {
 public:
  CanFreezePerTypeTracker() = default;
  ~CanFreezePerTypeTracker() = default;

  friend bool operator==(const CanFreezePerTypeTracker&,
                         const CanFreezePerTypeTracker&) = default;

  void PopulateWithPageFreezingState(const PageFreezingState& state) {
    for (auto freezing_type : FreezingTypeSet::All()) {
      if (HasCannotFreezeReasonForType(state.cannot_freeze_reasons,
                                       freezing_type)) {
        can_freeze_.Remove(freezing_type);
      }
    }
  }

  // Returns true if no `PageFreezeState` passed to
  // PopulateWithPageFreezingState() has a `CannotFreezeReason` applicable to
  // `type`.
  bool CanFreeze(FreezingType type) const { return can_freeze_.Has(type); }

  // Returns a `CanFreeze` enum value indicating whether CanFreeze() would
  // return true for all, some or no `FreezingType`.
  freezing::CanFreeze GetCanFreezeAllTypes() const {
    if (can_freeze_ == FreezingTypeSet::All()) {
      return freezing::CanFreeze::kYes;
    } else if (can_freeze_.empty()) {
      return freezing::CanFreeze::kNo;
    } else {
      return freezing::CanFreeze::kVaries;
    }
  }

 private:
  // Contains the `FreezingType`s for which no `CannotFreezeReason` prevents
  // freezing.
  FreezingTypeSet can_freeze_ = FreezingTypeSet::All();
};

FreezingPolicy::FreezingPolicy(
    std::unique_ptr<freezing::Discarder> discarder,
    std::unique_ptr<freezing::OptOutChecker> opt_out_checker)
    : freezer_(std::make_unique<Freezer>()),
      discarder_(std::move(discarder)),
      opt_out_checker_(std::move(opt_out_checker)),
      cpu_proportion_tracker_(
          /*context_filter=*/base::NullCallback(),
          /*cpu_proportion_type=*/resource_attribution::CPUProportionTracker::
              CPUProportionType::kBackground) {
  if (base::FeatureList::IsEnabled(features::kCPUMeasurementInFreezingPolicy) ||
      base::FeatureList::IsEnabled(
          features::kMemoryMeasurementInFreezingPolicy)) {
    resource_attribution::QueryBuilder builder;
    builder.AddAllContextsOfType<OriginInBrowsingInstanceContext>();
    if (base::FeatureList::IsEnabled(
            features::kCPUMeasurementInFreezingPolicy)) {
      builder.AddResourceType(resource_attribution::ResourceType::kCPUTime);
    }
    if (base::FeatureList::IsEnabled(
            features::kMemoryMeasurementInFreezingPolicy)) {
      builder.AddResourceType(
          resource_attribution::ResourceType::kMemorySummary);
    }

    resource_usage_query_ = builder.CreateScopedQuery();
    resource_usage_query_observation_.Observe(&resource_usage_query_.value());
    resource_usage_query_->Start(kCPUMeasurementInterval);
  }
#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(
          features::kInfiniteTabsFreezingOnMemoryPressure)) {
    memory_check_timer_.Start(
        FROM_HERE,
        features::kInfiniteTabsFreezingOnMemoryPressureInterval.Get(), this,
        &FreezingPolicy::CheckMemoryPressureForFreezing);
  }
#endif
}

FreezingPolicy::~FreezingPolicy() = default;

void FreezingPolicy::ToggleFreezingOnBatterySaverMode(bool is_enabled) {
  is_battery_saver_active_ = is_enabled;
  // Update frozen state for all connected sets of pages (toggling the state of
  // battery saver mode can affect the frozen state of any connected set).
  UpdateAllPagesFrozenState();
}

void FreezingPolicy::AddFreezeVote(PageNode* page_node) {
  int prev_num_freeze_votes = GetFreezingState(page_node).num_freeze_votes++;
  // A browsing instance may be frozen if there is at least one freeze vote.
  // Therefore, it's only necessary to update the frozen state when adding the
  // first freeze vote.
  if (prev_num_freeze_votes == 0) {
    UpdateFrozenState(page_node);
  }
}

void FreezingPolicy::RemoveFreezeVote(PageNode* page_node) {
  int num_freeze_votes = --GetFreezingState(page_node).num_freeze_votes;
  // A browsing instance may be frozen if there is at least one freeze vote.
  // Therefore, it's only necessary to update the frozen state when removing the
  // last freeze vote.
  if (num_freeze_votes == 0) {
    UpdateFrozenState(page_node);
  }
}

freezing::CanFreezeDetails FreezingPolicy::GetCanFreezeDetails(
    const PageNode* page_node) {
  freezing::CanFreezeDetails details;
  CanFreezePerTypeTracker can_freeze_per_type_tracker;

  for (const PageNode* connected_page_node : GetConnectedPages(page_node)) {
    const auto& page_freezing_state = GetFreezingState(connected_page_node);
    can_freeze_per_type_tracker.PopulateWithPageFreezingState(
        page_freezing_state);
    if (connected_page_node == page_node) {
      details.cannot_freeze_reasons.PutAll(
          page_freezing_state.cannot_freeze_reasons);
    } else {
      details.cannot_freeze_reasons_connected_pages.PutAll(
          page_freezing_state.cannot_freeze_reasons);
    }
  }

  details.can_freeze = can_freeze_per_type_tracker.GetCanFreezeAllTypes();
  return details;
}

// static
CannotFreezeReasonSet FreezingPolicy::CannotFreezeReasonsForType(
    FreezingPolicy::FreezingType type) {
  auto reasons = CannotFreezeReasonSet::All();
  if (type == FreezingPolicy::FreezingType::kInfiniteTabs) {
    // "Infinite tabs freezing" aims to have at most
    // `kInfiniteTabsFreezing_NumProtectedTabs` unfrozen. To keep that promise
    // even after the tab strip is traversed (e.g. by a user looking for a tab),
    // ignore `CannotFreezeReason::kRecentlyVisible`.
    reasons.Remove(CannotFreezeReason::kRecentlyVisible);
  } else {
    reasons.Remove(CannotFreezeReason::kMostRecentlyUsed);
  }
  return reasons;
}

FreezingPolicy::BrowsingInstanceState::BrowsingInstanceState() = default;
FreezingPolicy::BrowsingInstanceState::~BrowsingInstanceState() = default;

bool FreezingPolicy::BrowsingInstanceState::AllPagesFrozen() const {
  CHECK(!pages.empty());

  // This policy always requests the same frozen state for all pages in a
  // browsing instance. However, since freezing/unfreezing is done
  // asynchronously, pages may be in different state at a given point in time.

  for (const PageNode* page_node : pages) {
    if (page_node->GetLifecycleState() != PageNode::LifecycleState::kFrozen) {
      return false;
    }
  }
  return true;
}

base::flat_set<raw_ptr<const PageNode>> FreezingPolicy::GetConnectedPages(
    const PageNode* page) {
  base::flat_set<raw_ptr<const PageNode>> connected_pages;
  base::flat_set<raw_ptr<const PageNode>> pages_to_visit({page});
  while (!pages_to_visit.empty()) {
    // Implementation detail: `pages_to_visit` is a `flat_set`, which is a
    // sorted vector. Removal is most efficient from the end, so always visit
    // the last page in the set.
    auto visited_page_it = pages_to_visit.end() - 1;
    const PageNode* visited_page = *visited_page_it;
    pages_to_visit.erase(visited_page_it);

    auto [_, inserted] = connected_pages.insert(visited_page);
    CHECK(inserted);

    for (auto browsing_instance_id : GetBrowsingInstances(visited_page)) {
      auto it = browsing_instance_states_.find(browsing_instance_id);
      CHECK(it != browsing_instance_states_.end());
      const BrowsingInstanceState& browsing_instance_state = it->second;
      for (auto* browsing_instance_page : browsing_instance_state.pages) {
        if (!base::Contains(connected_pages, browsing_instance_page)) {
          pages_to_visit.insert(browsing_instance_page);
        }
      }
    }
  }

  return connected_pages;
}

base::flat_set<content::BrowsingInstanceId>
FreezingPolicy::GetBrowsingInstances(const PageNode* page) const {
  base::flat_set<content::BrowsingInstanceId> browsing_instances;
  for (const FrameNode* frame_node : page->GetMainFrameNodes()) {
    browsing_instances.insert(frame_node->GetBrowsingInstanceId());
  }
  return browsing_instances;
}

FreezingPolicy::PageFreezingState& FreezingPolicy::GetFreezingState(
    const PageNode* page_node) const {
  auto& state =
      *PageFreezingState::GetOrCreate(PageNodeImpl::FromNode(page_node));
  if (!state.periodic_unfreeze_phase) {
    state.periodic_unfreeze_phase = GenerateRandomPeriodicUnfreezePhase();
  }
  return state;
}

void FreezingPolicy::UpdateFrozenState(
    const PageNode* page,
    base::TimeTicks now,
    base::flat_set<raw_ptr<const PageNode>>* connected_pages_out) {
  const base::flat_set<raw_ptr<const PageNode>> connected_pages =
      GetConnectedPages(page);

  // Determine whether:
  // - Any connected page has a `CannotFreezeReason`.
  // - Any browsing instance hosting a frame from a connected page was CPU
  //   intensive in the background and Battery Saver is active and the
  //   `kFreezingOnBatterySaver` feature is enabled.
  // - Any connected page is in a periodic unfreeze period.
  // - All connected page have a freeze vote.
  CanFreezePerTypeTracker can_freeze_per_type_tracker;
  bool eligible_for_freezing_on_battery_saver = false;
  bool is_in_periodic_unfreeze = false;
  bool all_pages_have_freeze_vote = true;

  const double high_cpu_proportion = features::kFreezingHighCPUProportion.Get();

  for (const PageNode* visited_page : connected_pages) {
    auto& page_freezing_state = GetFreezingState(visited_page);

    can_freeze_per_type_tracker.PopulateWithPageFreezingState(
        page_freezing_state);

    if (page_freezing_state.num_freeze_votes == 0) {
      all_pages_have_freeze_vote = false;
    }

    if (page_freezing_state.IsInUnfreezePeriod(now)) {
      is_in_periodic_unfreeze = true;
    }

    for (auto browsing_instance_id : GetBrowsingInstances(visited_page)) {
      auto it = browsing_instance_states_.find(browsing_instance_id);
      CHECK(it != browsing_instance_states_.end());
      const BrowsingInstanceState& browsing_instance_state = it->second;

      if (browsing_instance_state
                  .highest_cpu_without_battery_saver_cannot_freeze >=
              high_cpu_proportion &&
          is_battery_saver_active_ &&
          // Note: Feature state is checked last so that only clients that
          // have a browsing instance that is CPU intensive in background
          // while Battery Saver is active are enrolled in the experiment.
          base::FeatureList::IsEnabled(features::kFreezingOnBatterySaver)) {
        eligible_for_freezing_on_battery_saver = true;
      }

      if (base::FeatureList::IsEnabled(
              features::kFreezingOnBatterySaverForTesting)) {
        eligible_for_freezing_on_battery_saver = true;
      }
    }
  }

  bool should_be_frozen = false;
  if (all_pages_have_freeze_vote &&
      can_freeze_per_type_tracker.CanFreeze(FreezingType::kVoting)) {
    should_be_frozen = true;
  } else if (eligible_for_freezing_on_battery_saver &&
             can_freeze_per_type_tracker.CanFreeze(
                 FreezingType::kBatterySaver)) {
    should_be_frozen = true;
  } else if (can_freeze_per_type_tracker.CanFreeze(
                 FreezingType::kInfiniteTabs) &&
             !is_in_periodic_unfreeze &&
             base::FeatureList::IsEnabled(features::kInfiniteTabsFreezing)) {
    should_be_frozen = true;
  } else if (can_freeze_per_type_tracker.CanFreeze(
                 FreezingType::kInfiniteTabs) &&
             !is_in_periodic_unfreeze && is_under_memory_pressure_ &&
             base::FeatureList::IsEnabled(
                 features::kInfiniteTabsFreezingOnMemoryPressure)) {
    should_be_frozen = true;
  }

  // Freeze/unfreeze connected pages as needed.
  for (const PageNode* connected_page : connected_pages) {
    auto& page_freezing_state = GetFreezingState(connected_page);
    if (page_freezing_state.frozen == should_be_frozen) {
      continue;
    }
    if (should_be_frozen) {
      freezer_->MaybeFreezePageNode(connected_page);
    } else {
      freezer_->UnfreezePageNode(connected_page);
    }
    page_freezing_state.frozen = should_be_frozen;
  }

  if (connected_pages_out) {
    connected_pages_out->insert(connected_pages.begin(), connected_pages.end());
  }
}

void FreezingPolicy::OnCannotFreezeReasonChange(const PageNode* page_node,
                                                bool add,
                                                CannotFreezeReason reason) {
  auto& state = GetFreezingState(page_node);
  CanFreezePerTypeTracker before_tracker;
  before_tracker.PopulateWithPageFreezingState(state);

  if (add) {
    DCHECK(!state.cannot_freeze_reasons.Has(reason));
    state.cannot_freeze_reasons.Put(reason);

    // Track that the browsing instance had a `CannotFreezeReason`, so that the
    // next CPU measurement for it can be ignored (this bit is sticky and won't
    // be reset if the `CannotFreezeReason` is removed before the next
    // measurement).
    for (auto browsing_instance_id : GetBrowsingInstances(page_node)) {
      auto it = browsing_instance_states_.find(browsing_instance_id);
      CHECK(it != browsing_instance_states_.end());
      it->second.cannot_freeze_reasons_since_last_cpu_measurement.Put(reason);
    }
  } else {
    DCHECK(state.cannot_freeze_reasons.Has(reason));
    state.cannot_freeze_reasons.Remove(reason);
  }

  CanFreezePerTypeTracker after_tracker;
  after_tracker.PopulateWithPageFreezingState(state);

  const base::TimeTicks now = base::TimeTicks::Now();

  if (!after_tracker.CanFreeze(FreezingType::kInfiniteTabs)) {
    // No need to run the periodic unfreeze timer when the tab isn't eligible
    // for infinite tabs freezing.
    state.periodic_unfreeze_timer.Stop();
  } else if (!state.periodic_unfreeze_timer.IsRunning()) {
    // Start a timer which fires when entering or exiting a periodic unfreeze
    // period.
    StartPeriodicUnfreezeTimer(page_node, now);
  }

  if (before_tracker != after_tracker) {
    UpdateFrozenState(page_node);
  }
}

CannotFreezeReasonSet FreezingPolicy::GetCannotFreezeReasons(
    const BrowsingInstanceState& browsing_instance_state) {
  CannotFreezeReasonSet reasons;
  for (const PageNode* page : browsing_instance_state.pages) {
    const auto& page_freezing_state = GetFreezingState(page);
    reasons.PutAll(page_freezing_state.cannot_freeze_reasons);
  }
  return reasons;
}

void FreezingPolicy::OnPassedToGraph(Graph* graph) {
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this, "Freezing");
  if (opt_out_checker_) {
    opt_out_checker_->SetOptOutPolicyChangedCallback(base::BindRepeating(
        &FreezingPolicy::OnOptOutPolicyChanged, weak_factory_.GetWeakPtr()));
  }
}

void FreezingPolicy::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
}

void FreezingPolicy::OnPageNodeAdded(const PageNode* page_node) {
  auto& page_freezing_state = GetFreezingState(page_node);

  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)->AddObserver(
      this);

  if (page_node->IsVisible()) {
    page_freezing_state.cannot_freeze_reasons.Put(CannotFreezeReason::kVisible);
  }

  if (page_node->IsAudible()) {
    page_freezing_state.cannot_freeze_reasons.Put(CannotFreezeReason::kAudible);
  }

  if (opt_out_checker_ &&
      opt_out_checker_->IsPageOptedOutOfFreezing(
          page_node->GetBrowserContextID(), page_node->GetMainFrameUrl())) {
    page_freezing_state.cannot_freeze_reasons.Put(
        CannotFreezeReason::kOptedOut);
  }

  DCHECK(!page_node->HasFreezingOriginTrialOptOut());
  DCHECK(!page_node->UsesWebRTC());
  DCHECK(!page_node->GetNotificationPermissionStatus().has_value());
  DCHECK(!page_node->IsHoldingWebLock());
  DCHECK(!page_node->IsHoldingBlockingIndexedDBLock());
  DCHECK_EQ(page_node->GetLoadingState(),
            PageNode::LoadingState::kLoadingNotStarted);
  DCHECK(!IsPageConnectedToUSBDevice(page_node));
  DCHECK(!IsPageConnectedToBluetoothDevice(page_node));
  DCHECK(!IsPageConnectedToHidDevice(page_node));
  DCHECK(!IsPageConnectedToSerialPort(page_node));
  DCHECK(!IsPageCapturingVideo(page_node));
  DCHECK(!IsPageCapturingAudio(page_node));
  DCHECK(!IsPageBeingMirrored(page_node));
  DCHECK(!IsPageCapturingWindow(page_node));
  DCHECK(!IsPageCapturingDisplay(page_node));

  // No need to update the frozen state, because the page doesn't have frames
  // yet and thus doesn't belong to any browsing instance.
  CHECK(page_node->GetMainFrameNodes().empty());
}

void FreezingPolicy::OnBeforePageNodeRemoved(const PageNode* page_node) {
  if (page_node->GetType() == PageType::kTab) {
    if (page_node->IsVisible()) {
      CHECK_GT(num_visible_tabs_, 0, base::NotFatalUntil::M140);
      --num_visible_tabs_;
    } else {
      std::erase(most_recently_used_, page_node);
    }
  }

  CHECK(page_node->GetMainFrameNodes().empty());
  CHECK(!base::Contains(most_recently_used_, page_node),
        base::NotFatalUntil::M140);
  CheckMostRecentlyUsedListSize();
}

void FreezingPolicy::OnTypeChanged(const PageNode* page_node,
                                   PageType previous_type) {
  CHECK_EQ(previous_type, PageType::kUnknown, base::NotFatalUntil::M140);
  if (page_node->GetType() != PageType::kTab) {
    return;
  }

  if (page_node->IsVisible()) {
    ++num_visible_tabs_;
  } else {
    // When a tab is created in the background (e.g. Open Link in New Tab), we
    // assume that the user cares about it more than tabs visited a while ago,
    // so we add it to the front of the most recently used list. UXR could
    // motivate a different approach.
    most_recently_used_.push_front(page_node);
    OnCannotFreezeReasonChange(page_node, /*add=*/true,
                               CannotFreezeReason::kMostRecentlyUsed);
  }
  MaybePopFromMostRecentlyUsedList();
}

void FreezingPolicy::OnIsVisibleChanged(const PageNode* page_node) {
  auto& page_freezing_state = GetFreezingState(page_node);
  if (page_node->IsVisible()) {
    // Page becomes visible.
    OnCannotFreezeReasonChange(page_node, /*add=*/true,
                               CannotFreezeReason::kVisible);
    if (page_freezing_state.recently_visible_timer.IsRunning()) {
      page_freezing_state.recently_visible_timer.Stop();
      OnCannotFreezeReasonChange(page_node, /*add=*/false,
                                 CannotFreezeReason::kRecentlyVisible);
    }

    if (page_node->GetType() == PageType::kTab) {
      ++num_visible_tabs_;
      size_t num_erased = std::erase(most_recently_used_, page_node);
      if (num_erased == 0) {
        MaybePopFromMostRecentlyUsedList();
      } else {
        OnCannotFreezeReasonChange(page_node, /*add=*/false,
                                   CannotFreezeReason::kMostRecentlyUsed);
      }
    }
  } else {
    // Page becomes hidden.
    if (page_node->GetType() == PageType::kTab) {
      CHECK(!base::Contains(most_recently_used_, page_node),
            base::NotFatalUntil::M140);
      CHECK_GT(num_visible_tabs_, 0, base::NotFatalUntil::M140);
      --num_visible_tabs_;
      most_recently_used_.push_front(page_node);
      OnCannotFreezeReasonChange(page_node, /*add=*/true,
                                 CannotFreezeReason::kMostRecentlyUsed);
      MaybePopFromMostRecentlyUsedList();
    }

    OnCannotFreezeReasonChange(page_node, /*add=*/true,
                               CannotFreezeReason::kRecentlyVisible);
    OnCannotFreezeReasonChange(page_node, /*add=*/false,
                               CannotFreezeReason::kVisible);

    page_freezing_state.recently_visible_timer.Start(
        FROM_HERE, features::kFreezingVisibleProtectionTime.Get(),
        base::BindOnce(
            &FreezingPolicy::OnCannotFreezeReasonChange, base::Unretained(this),
            // Safe because the `PageFreezingState` and the timer are deleted by
            // `OnBeforePageNodeRemoved` before `page_node` is deleted.
            base::Unretained(page_node),
            /* add=*/false, CannotFreezeReason::kRecentlyVisible));
  }
  CheckMostRecentlyUsedListSize();
}

void FreezingPolicy::OnIsAudibleChanged(const PageNode* page_node) {
  auto& page_freezing_state = GetFreezingState(page_node);
  if (page_node->IsAudible()) {
    OnCannotFreezeReasonChange(page_node, /*add=*/true,
                               CannotFreezeReason::kAudible);
    if (page_freezing_state.recently_audible_timer.IsRunning()) {
      page_freezing_state.recently_audible_timer.Stop();
      OnCannotFreezeReasonChange(page_node, /*add=*/false,
                                 CannotFreezeReason::kRecentlyAudible);
    }
  } else {
    OnCannotFreezeReasonChange(page_node, /*add=*/true,
                               CannotFreezeReason::kRecentlyAudible);
    OnCannotFreezeReasonChange(page_node, /*add=*/false,
                               CannotFreezeReason::kAudible);

    page_freezing_state.recently_audible_timer.Start(
        FROM_HERE, features::kFreezingAudioProtectionTime.Get(),
        base::BindOnce(
            &FreezingPolicy::OnCannotFreezeReasonChange, base::Unretained(this),
            // Safe because the `PageFreezingState` and the timer are deleted by
            // `OnBeforePageNodeRemoved` before `page_node` is deleted.
            base::Unretained(page_node),
            /* add=*/false, CannotFreezeReason::kRecentlyAudible));
  }
}

void FreezingPolicy::OnPageLifecycleStateChanged(const PageNode* page_node) {
  // When a page is unfrozen, clear post-freezing PMF measurements for all its
  // browsing instances.
  if (page_node->GetLifecycleState() != PageNode::LifecycleState::kFrozen) {
    for (content::BrowsingInstanceId id : GetBrowsingInstances(page_node)) {
      auto it = browsing_instance_states_.find(id);
      CHECK(it != browsing_instance_states_.end());
      it->second.per_origin_pmf_after_freezing.clear();
    }
  }
}

void FreezingPolicy::OnPageHasFreezingOriginTrialOptOutChanged(
    const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node,
                             /*add=*/page_node->HasFreezingOriginTrialOptOut(),
                             CannotFreezeReason::kFreezingOriginTrialOptOut);
}

void FreezingPolicy::OnPageUsesWebRTCChanged(const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node, /*add=*/page_node->UsesWebRTC(),
                             CannotFreezeReason::kWebRTC);
}

void FreezingPolicy::OnPageNotificationPermissionStatusChange(
    const PageNode* page_node,
    std::optional<blink::mojom::PermissionStatus> previous_status) {
  const bool had_notification_permission =
      previous_status.value_or(blink::mojom::PermissionStatus::DENIED) ==
      blink::mojom::PermissionStatus::GRANTED;
  const auto new_status = page_node->GetNotificationPermissionStatus();
  const bool has_notification_permission =
      new_status.value_or(blink::mojom::PermissionStatus::DENIED) ==
      blink::mojom::PermissionStatus::GRANTED;

  if (had_notification_permission == has_notification_permission) {
    return;
  }

  OnCannotFreezeReasonChange(page_node, /*add=*/has_notification_permission,
                             CannotFreezeReason::kNotificationPermission);
}

void FreezingPolicy::OnPageIsHoldingWebLockChanged(const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node, /*add=*/page_node->IsHoldingWebLock(),
                             CannotFreezeReason::kHoldingWebLock);
}

void FreezingPolicy::OnMainFrameUrlChanged(const PageNode* page_node) {
  const bool was_opted_out =
      GetFreezingState(page_node).cannot_freeze_reasons.Has(
          CannotFreezeReason::kOptedOut);
  const bool is_opted_out =
      opt_out_checker_ &&
      opt_out_checker_->IsPageOptedOutOfFreezing(
          page_node->GetBrowserContextID(), page_node->GetMainFrameUrl());
  if (was_opted_out != is_opted_out) {
    OnCannotFreezeReasonChange(page_node, /*add=*/is_opted_out,
                               CannotFreezeReason::kOptedOut);
  }
}

void FreezingPolicy::OnPageIsHoldingBlockingIndexedDBLockChanged(
    const PageNode* page_node) {
  OnCannotFreezeReasonChange(
      page_node,
      /*add=*/page_node->IsHoldingBlockingIndexedDBLock(),
      CannotFreezeReason::kHoldingBlockingIndexedDBLock);
}

void FreezingPolicy::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  auto cannot_freeze = [](PageNode::LoadingState loading_state) {
    // Using a switch statement to force this logic to be updated when
    // `LoadingState` values are updated.
    switch (loading_state) {
      case PageNode::LoadingState::kLoadingNotStarted:
      case PageNode::LoadingState::kLoadedIdle:
        return false;
      case PageNode::LoadingState::kLoading:
      case PageNode::LoadingState::kLoadingTimedOut:
      case PageNode::LoadingState::kLoadedBusy:
        return true;
    }
  };

  const bool previous_cannot_freeze = cannot_freeze(previous_state);
  const bool new_cannot_freeze = cannot_freeze(page_node->GetLoadingState());

  if (previous_cannot_freeze == new_cannot_freeze) {
    return;
  }

  OnCannotFreezeReasonChange(page_node,
                             /*add=*/new_cannot_freeze,
                             CannotFreezeReason::kLoading);
}

void FreezingPolicy::OnFrameNodeAdded(const FrameNode* frame_node) {
  if (!frame_node->IsMainFrame()) {
    return;
  }

  // Associate the frame's page with the frame's browsing instance, if not
  // already associated.
  auto& browsing_instance_state =
      browsing_instance_states_[frame_node->GetBrowsingInstanceId()];
  auto [it, inserted] =
      browsing_instance_state.pages.insert(frame_node->GetPageNode());
  if (!inserted) {
    return;
  }

  // Clear `per_origin_pmf_after_freezing` since not all pages in the
  // browsing instance are frozen when a new page is added.
  CHECK_EQ(frame_node->GetLifecycleState(), FrameNode::LifecycleState::kRunning,
           base::NotFatalUntil::M140);
  browsing_instance_state.per_origin_pmf_after_freezing.clear();

  // Update frozen state for browsing instances associated with the frame's
  // page.
  UpdateFrozenState(frame_node->GetPageNode());
}

void FreezingPolicy::OnFrameNodeRemoved(
    const FrameNode* frame_node,
    const FrameNode* previous_parent_frame_node,
    const PageNode* previous_page_node,
    const ProcessNode* previous_process_node,
    const FrameNode* previous_parent_or_outer_document_or_embedder) {
  if (!frame_node->IsMainFrame()) {
    return;
  }

  // Early exit if another main frame is associated with the same browsing
  // instance (in other words, the set of browsing instances associated with the
  // removed frame's page doesn't change).
  for (const FrameNode* other_frame_node :
       previous_page_node->GetMainFrameNodes()) {
    CHECK_NE(other_frame_node, frame_node);
    if (other_frame_node->GetBrowsingInstanceId() ==
        frame_node->GetBrowsingInstanceId()) {
      return;
    }
  }

  // Disassociate the frame's page from the frame's browsing instance.
  auto it = browsing_instance_states_.find(frame_node->GetBrowsingInstanceId());
  CHECK(it != browsing_instance_states_.end());
  size_t num_pages_removed = it->second.pages.erase(previous_page_node);
  CHECK_EQ(num_pages_removed, 1U);

  // Update frozen state for pages connected to the frame's page, if it still
  // contains at least one frame.
  if (!previous_page_node->GetMainFrameNodes().empty()) {
    UpdateFrozenState(previous_page_node);
  }

  // If pages remain in the deleted frame's browsing instance, update their
  // frozen state (note: these pages may no longer be connected to the frame's
  // page). Otherwise, delete the deleted frame's browsing instance state.
  if (it->second.pages.empty()) {
    browsing_instance_states_.erase(it);
  } else {
    UpdateFrozenState(*it->second.pages.begin());
  }
}

void FreezingPolicy::OnIsAudibleChanged(const FrameNode* frame_node) {}

void FreezingPolicy::OnIsConnectedToUSBDeviceChanged(
    const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node,
                             /*add=*/IsPageConnectedToUSBDevice(page_node),
                             CannotFreezeReason::kConnectedToUsbDevice);
}

void FreezingPolicy::OnIsConnectedToBluetoothDeviceChanged(
    const PageNode* page_node) {
  OnCannotFreezeReasonChange(
      page_node, /*add=*/IsPageConnectedToBluetoothDevice(page_node),
      CannotFreezeReason::kConnectedToBluetoothDevice);
}

void FreezingPolicy::OnIsConnectedToHidDeviceChanged(
    const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node,
                             /*add=*/IsPageConnectedToHidDevice(page_node),
                             CannotFreezeReason::kConnectedToHidDevice);
}

void FreezingPolicy::OnIsConnectedToSerialPortChanged(
    const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node,
                             /*add=*/IsPageConnectedToSerialPort(page_node),
                             CannotFreezeReason::kConnectedToSerialPort);
}

void FreezingPolicy::OnIsCapturingVideoChanged(const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node, /*add=*/IsPageCapturingVideo(page_node),
                             CannotFreezeReason::kCapturingVideo);
}

void FreezingPolicy::OnIsCapturingAudioChanged(const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node, /*add=*/IsPageCapturingAudio(page_node),
                             CannotFreezeReason::kCapturingAudio);
}

void FreezingPolicy::OnIsBeingMirroredChanged(const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node, /*add=*/IsPageBeingMirrored(page_node),
                             CannotFreezeReason::kBeingMirrored);
}

void FreezingPolicy::OnIsCapturingWindowChanged(const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node,
                             /*add=*/IsPageCapturingWindow(page_node),
                             CannotFreezeReason::kCapturingWindow);
}

void FreezingPolicy::OnIsCapturingDisplayChanged(const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node,
                             /*add=*/IsPageCapturingDisplay(page_node),
                             CannotFreezeReason::kCapturingDisplay);
}

base::Value::Dict FreezingPolicy::DescribePageNodeData(
    const PageNode* node) const {
  base::Value::Dict ret;

  const auto& page_freezing_state = GetFreezingState(node);

  // Present number of freeze votes for this page.
  ret.Set("num_freeze_votes", page_freezing_state.num_freeze_votes);

  // Present browsing instances for this page.
  {
    base::Value::List browsing_instances;
    for (auto browsing_instance : GetBrowsingInstances(node)) {
      browsing_instances.Append(browsing_instance.value());
    }
    ret.Set("browsing_instances", std::move(browsing_instances));
  }

  // Present `CannotFreezeReason`s for this page.
  {
    base::Value::List cannot_freeze_reasons_list;
    for (auto reason : page_freezing_state.cannot_freeze_reasons) {
      cannot_freeze_reasons_list.Append(CannotFreezeReasonToString(reason));
    }
    if (!cannot_freeze_reasons_list.empty()) {
      ret.Set("cannot_freeze_reasons", std::move(cannot_freeze_reasons_list));
    }
  }

  // Present `CannotFreezeReason`s for other pages in browsing instances
  // associated with this page. They affect freezing for this page.
  {
    CannotFreezeReasonSet cannot_freeze_reasons_other_pages;
    for (auto browsing_instance : GetBrowsingInstances(node)) {
      auto browsing_instance_it =
          browsing_instance_states_.find(browsing_instance);
      CHECK(browsing_instance_it != browsing_instance_states_.end());
      for (const PageNode* other_page_node :
           browsing_instance_it->second.pages) {
        if (other_page_node != node) {
          cannot_freeze_reasons_other_pages.PutAll(
              GetFreezingState(other_page_node).cannot_freeze_reasons);
        }
      }
    }
    base::Value::List cannot_freeze_reasons_other_pages_list;
    for (CannotFreezeReason reason : cannot_freeze_reasons_other_pages) {
      cannot_freeze_reasons_other_pages_list.Append(
          CannotFreezeReasonToString(reason));
    }

    if (!cannot_freeze_reasons_other_pages_list.empty()) {
      ret.Set("cannot_freeze_reasons_other_pages",
              std::move(cannot_freeze_reasons_other_pages_list));
    }
  }

  return ret;
}

void FreezingPolicy::OnResourceUsageUpdated(
    const resource_attribution::QueryResultMap& results) {
  DiscardFrozenPagesWithGrowingMemoryOnMemoryMeasurement(results);
  UpdateFrozenStateOnCPUMeasurement(results);
}

void FreezingPolicy::DiscardFrozenPagesWithGrowingMemoryOnMemoryMeasurement(
    const resource_attribution::QueryResultMap& results) {
  // Clear memory measurements for browsing instances that are not frozen and
  // build a set of frozen browsing instances that don't yet have their initial
  // memory measurement.
  std::set<content::BrowsingInstanceId>
      browsing_instance_states_without_initial_measurement;
  for (auto& [id, state] : browsing_instance_states_) {
    if (state.AllPagesFrozen()) {
      if (state.per_origin_pmf_after_freezing.empty()) {
        browsing_instance_states_without_initial_measurement.insert(id);
      }
    } else {
      // Should have been cleared by OnPageLifecycleStateChanged() or
      // OnFrameNodeAdded().
      CHECK(state.per_origin_pmf_after_freezing.empty(),
            base::NotFatalUntil::M140);
    }
  }

  const base::ByteCount growth_threshold =
      base::KiB(features::kFreezingMemoryGrowthThresholdToDiscardKb.Get());

  // Traverse memory measurements to find pages to discard.
  std::set<const PageNode*> pages_to_discard;
  for (const auto& [context, result] : results) {
    if (!result.memory_summary_result.has_value()) {
      continue;
    }

    // This cast is valid because the query only targets contexts of type
    // `OriginInBrowsingInstanceContext` (verified by CHECK inside `AsContext`).
    const auto& origin_in_browsing_instance_context =
        resource_attribution::AsContext<OriginInBrowsingInstanceContext>(
            context);
    auto browsing_instance_it = browsing_instance_states_.find(
        origin_in_browsing_instance_context.GetBrowsingInstance());
    if (browsing_instance_it == browsing_instance_states_.end()) {
      continue;
    }
    content::BrowsingInstanceId id = browsing_instance_it->first;
    auto& state = browsing_instance_it->second;

    if (!state.AllPagesFrozen()) {
      continue;
    }

    const base::ByteCount current =
        result.memory_summary_result->private_footprint;
    if (base::Contains(browsing_instance_states_without_initial_measurement,
                       id)) {
      // Store the first PMF measurement after being frozen.
      state.per_origin_pmf_after_freezing[origin_in_browsing_instance_context
                                              .GetOrigin()] = current;
    } else {
      // Compare current measurement against the one stored after being frozen.
      auto it = state.per_origin_pmf_after_freezing.find(
          origin_in_browsing_instance_context.GetOrigin());
      base::ByteCount after_freezing;
      if (it == state.per_origin_pmf_after_freezing.end()) {
        // No memory measurement was stored for this origin after being frozen.
        // This could indicate a measurement error (e.g. process missing in a
        // `memory_instrumentation::GlobalMemoryDump`) or that the browsing
        // instance navigated while frozen (e.g. requested by an extension).
        //
        // Pretend that 0 was stored after freezing. This will cause the
        // browsing instance to be discarded iff the current measurement is
        // above the growth threshold. In any case, no extra measurement is
        // stored in `per_origin_pmf_after_freezing` to prevent it from
        // growing without bounds if the page continuously navigates to new
        // origins.
        after_freezing = base::ByteCount(0);
      } else {
        after_freezing = it->second;
      }

      const base::ByteCount growth = current > after_freezing
                                         ? current - after_freezing
                                         : base::ByteCount(0);
      if (growth > growth_threshold) {
        pages_to_discard.insert(state.pages.begin(), state.pages.end());
      }
    }
  }

  // Note: Feature state is checked last so that only clients that have a frozen
  // browsing instance with a growing private memory footprint are enrolled in
  // the experiment.
  if (!pages_to_discard.empty() &&
      base::FeatureList::IsEnabled(
          features::kDiscardFrozenBrowsingInstancesWithGrowingPMF)) {
    discarder_->DiscardPages(
        GetOwningGraph(), std::vector<const PageNode*>(pages_to_discard.begin(),
                                                       pages_to_discard.end()));
  }
}

void FreezingPolicy::UpdateFrozenStateOnCPUMeasurement(
    const resource_attribution::QueryResultMap& results) {
  if (!cpu_proportion_tracker_.IsTracking()) {
    cpu_proportion_tracker_.StartFirstInterval(base::TimeTicks::Now(), results);
    return;
  }

  const double high_cpu_proportion = features::kFreezingHighCPUProportion.Get();
  const std::map<resource_attribution::ResourceContext, double>
      cpu_proportion_map = cpu_proportion_tracker_.StartNextInterval(
          base::TimeTicks::Now(), results);
  for (const auto& [context, cpu_proportion] : cpu_proportion_map) {
    // This cast is valid because the query only targets contexts of type
    // `OriginInBrowsingInstanceContext` (verified by CHECK inside `AsContext`).
    const auto& origin_in_browsing_instance_context =
        resource_attribution::AsContext<OriginInBrowsingInstanceContext>(
            context);
    auto browsing_instance_it = browsing_instance_states_.find(
        origin_in_browsing_instance_context.GetBrowsingInstance());
    if (browsing_instance_it == browsing_instance_states_.end()) {
      continue;
    }

    BrowsingInstanceState& state = browsing_instance_it->second;
    state.highest_cpu_current_interval = std::max(
        state.highest_cpu_current_interval.value_or(0), cpu_proportion);

    if (HasCannotFreezeReasonForType(
            state.cannot_freeze_reasons_since_last_cpu_measurement,
            FreezingType::kBatterySaver)) {
      // Ignore CPU measurement while having a `CannotFreezeReason` applicable
      // to `kBatterySaver` (it's acceptable to use a lot of CPU
      // while playing audio, running a videoconference call...).
      continue;
    }

    if (state.highest_cpu_without_battery_saver_cannot_freeze >
        cpu_proportion) {
      // Ignore CPU measurement without a `CannotFreezeReason` if it's not the
      // highest one.
      continue;
    }

    // Store the new highest CPU measurement without a `CannotFreezeReason`.
    state.highest_cpu_without_battery_saver_cannot_freeze = cpu_proportion;

    // If the CPU measurement is above the threshold for high CPU usage, update
    // the frozen state.
    if (cpu_proportion >= high_cpu_proportion) {
      UpdateFrozenState(*state.pages.begin());
    }
  }

  // Report UKM for all pages.
  RecordFreezingEligibilityUKM();

  // Reset state for the next interval for all browsing instances.
  for (auto& [_, state] : browsing_instance_states_) {
    state.cannot_freeze_reasons_since_last_cpu_measurement =
        GetCannotFreezeReasons(state);
    state.highest_cpu_current_interval.reset();
  }
}

void FreezingPolicy::OnOptOutPolicyChanged(
    std::string_view browser_context_id) {
  CHECK(opt_out_checker_);
  // Check all pages  with the given `browser_context_id` to see if they're
  // opted out of freezing by the new policy.
  for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
    if (page_node->GetBrowserContextID() != browser_context_id) {
      continue;
    }
    const bool was_opted_out =
        GetFreezingState(page_node).cannot_freeze_reasons.Has(
            CannotFreezeReason::kOptedOut);
    const bool is_opted_out = opt_out_checker_->IsPageOptedOutOfFreezing(
        browser_context_id, page_node->GetMainFrameUrl());
    if (was_opted_out != is_opted_out) {
      OnCannotFreezeReasonChange(page_node, /*add=*/is_opted_out,
                                 CannotFreezeReason::kOptedOut);
    }
  }
}

void FreezingPolicy::MaybePopFromMostRecentlyUsedList() {
  const int num_protected_tabs =
      num_visible_tabs_ + base::checked_cast<int>(most_recently_used_.size());

  if (num_protected_tabs <=
          features::kInfiniteTabsFreezing_NumProtectedTabs.Get() ||
      most_recently_used_.empty()) {
    return;
  }

  const PageNode* back = most_recently_used_.back();
  most_recently_used_.pop_back();
  OnCannotFreezeReasonChange(back, /*add=*/false,
                             CannotFreezeReason::kMostRecentlyUsed);

  // Check that removing one tab from `most_recently_used_` was sufficient to
  // respect the limit. This should be the case if this method is called
  // whenever `num_visible_tabs_` is incremented or an element is added to
  // `most_recently_used_`.
  CheckMostRecentlyUsedListSize();
}

void FreezingPolicy::CheckMostRecentlyUsedListSize() {
  // If the most recently used list is empty, `num_visible_tabs_` may exceed the
  // limit (there is no cap on the number of visible tabs).
  if (most_recently_used_.empty()) {
    return;
  }

  // Otherwise, the sum of the most recently used list size and the number of
  // visible tabs must be below or at the limit.
  CHECK_LE(
      num_visible_tabs_ + base::checked_cast<int>(most_recently_used_.size()),
      features::kInfiniteTabsFreezing_NumProtectedTabs.Get(),
      base::NotFatalUntil::M140);
}

void FreezingPolicy::StartPeriodicUnfreezeTimer(const PageNode* page_node,
                                                base::TimeTicks now) {
  auto& state = GetFreezingState(page_node);
  CHECK(!state.periodic_unfreeze_timer.IsRunning(), base::NotFatalUntil::M141);
  state.periodic_unfreeze_timer.Start(
      FROM_HERE, state.GetDelayUntilNextUnfreezeStateChange(now),
      base::BindOnce(&FreezingPolicy::OnPeriodicUnfreezeTimer,
                     base::Unretained(this), base::Unretained(page_node)));
}

void FreezingPolicy::OnPeriodicUnfreezeTimer(const PageNode* page_node) {
  const base::TimeTicks now = base::TimeTicks::Now();
  UpdateFrozenState(page_node, now);
  StartPeriodicUnfreezeTimer(page_node, now);
}

void FreezingPolicy::RecordFreezingEligibilityUKM() {
  if (!base::FeatureList::IsEnabled(features::kRecordFreezingEligibilityUKM)) {
    return;
  }

  // This function is about to potentially emit many UKM events (roughly 1 per
  // existing page). If this is done too often, the UKM recorder system itself
  // will start subsampling those UKM events. Thus, it's better to subsample the
  // event emission code itself to increase the proportion of emitted events
  // that are actually recorded.
  if (!metrics_subsampler_.ShouldSample(0.01)) {
    return;
  }

  base::flat_set<raw_ptr<const PageNode>> visited_pages;
  const auto cannot_freeze_reasons_for_battery_saver =
      CannotFreezeReasonsForType(FreezingType::kBatterySaver);

  for (auto* page : GetOwningGraph()->GetAllPageNodes()) {
    if (visited_pages.contains(page)) {
      // The page is part of a group of connected pages for which the UKM event
      // was already emitted.
      continue;
    }

    std::optional<double> highest_cpu_current_interval;
    double highest_cpu_without_battery_saver_cannot_freeze = 0.0;
    CannotFreezeReasonSet cannot_freeze_reasons;
    const auto connected_pages = GetConnectedPages(page);

    for (auto& connected_page : connected_pages) {
      for (auto browsing_instance_id : GetBrowsingInstances(connected_page)) {
        auto it = browsing_instance_states_.find(browsing_instance_id);
        CHECK(it != browsing_instance_states_.end());
        auto& state = it->second;

        if (state.highest_cpu_current_interval.has_value()) {
          highest_cpu_current_interval =
              std::max(highest_cpu_current_interval.value_or(0),
                       state.highest_cpu_current_interval.value());
        }

        highest_cpu_without_battery_saver_cannot_freeze =
            std::max(highest_cpu_without_battery_saver_cannot_freeze,
                     state.highest_cpu_without_battery_saver_cannot_freeze);
        cannot_freeze_reasons.PutAll(
            state.cannot_freeze_reasons_since_last_cpu_measurement);
      }
    }

    // Record the UKM event if there was a CPU measurement for this group of
    // connected pages.
    if (highest_cpu_current_interval.has_value()) {
      for (auto& connected_page : connected_pages) {
        RecordFreezingEligibilityUKMForPage(
            connected_page->GetUkmSourceID(),
            highest_cpu_current_interval.value(),
            highest_cpu_without_battery_saver_cannot_freeze,
            /*battery_saver_cannot_freeze_reasons=*/
            base::Intersection(cannot_freeze_reasons,
                               cannot_freeze_reasons_for_battery_saver));
      }
    }

    visited_pages.insert(connected_pages.begin(), connected_pages.end());
  }
}

void FreezingPolicy::RecordFreezingEligibilityUKMForPage(
    ukm::SourceId source_id,
    double highest_cpu_current_interval,
    double highest_cpu_without_battery_saver_cannot_freeze,
    CannotFreezeReasonSet battery_saver_cannot_freeze_reasons) {
  RecordFreezingEligibilityUKMForPageStatic(
      source_id, highest_cpu_current_interval,
      highest_cpu_without_battery_saver_cannot_freeze,
      battery_saver_cannot_freeze_reasons);
}

void FreezingPolicy::RecordFreezingEligibilityUKMForPageStatic(
    ukm::SourceId source_id,
    double highest_cpu_current_interval,
    double highest_cpu_without_battery_saver_cannot_freeze,
    CannotFreezeReasonSet battery_saver_cannot_freeze_reasons) {
  CHECK(
      base::Difference(battery_saver_cannot_freeze_reasons,
                       CannotFreezeReasonsForType(FreezingType::kBatterySaver))
          .empty());

  auto ukm = ukm::builders::PerformanceManager_FreezingEligibility(source_id);

  // The bucketing has this effect:
  //  0      = 0
  //  1      = 1
  //  2-3    = 2
  //  4-7    = 4
  //  8-15   = 8
  //  16-31  = 16
  //  32-63  = 32
  //  64-127 = 64
  //  ...
  //
  // This precision is sufficient for exploring the coverage of thresholds
  // between 2% and 25% as we plan to do.
  ukm.SetHighestCPUCurrentInterval(ukm::GetExponentialBucketMinForUserTiming(
      highest_cpu_current_interval * 100));
  ukm.SetHighestCPUAnyIntervalWithoutOptOut(
      ukm::GetExponentialBucketMinForUserTiming(
          highest_cpu_without_battery_saver_cannot_freeze * 100));

  ukm.SetVisible(
      battery_saver_cannot_freeze_reasons.Has(CannotFreezeReason::kVisible));
  ukm.SetRecentlyVisible(battery_saver_cannot_freeze_reasons.Has(
      CannotFreezeReason::kRecentlyVisible));
  ukm.SetAudible(
      battery_saver_cannot_freeze_reasons.Has(CannotFreezeReason::kAudible));
  ukm.SetRecentlyAudible(battery_saver_cannot_freeze_reasons.Has(
      CannotFreezeReason::kRecentlyAudible));
  ukm.SetOriginTrialOptOut(battery_saver_cannot_freeze_reasons.Has(
      CannotFreezeReason::kFreezingOriginTrialOptOut));
  ukm.SetHoldingWebLock(battery_saver_cannot_freeze_reasons.Has(
      CannotFreezeReason::kHoldingWebLock));
  ukm.SetHoldingBlockingIndexedDBLock(battery_saver_cannot_freeze_reasons.Has(
      CannotFreezeReason::kHoldingBlockingIndexedDBLock));
  ukm.SetConnectedToDevice(battery_saver_cannot_freeze_reasons.HasAny(
      {CannotFreezeReason::kConnectedToUsbDevice,
       CannotFreezeReason::kConnectedToBluetoothDevice,
       CannotFreezeReason::kConnectedToHidDevice,
       CannotFreezeReason::kConnectedToSerialPort}));
  ukm.SetCapturing(battery_saver_cannot_freeze_reasons.HasAny(
      {CannotFreezeReason::kCapturingAudio, CannotFreezeReason::kCapturingVideo,
       CannotFreezeReason::kCapturingWindow,
       CannotFreezeReason::kCapturingDisplay}));
  ukm.SetBeingMirrored(battery_saver_cannot_freeze_reasons.Has(
      CannotFreezeReason::kBeingMirrored));
  ukm.SetWebRTC(
      battery_saver_cannot_freeze_reasons.Has(CannotFreezeReason::kWebRTC));
  ukm.SetLoading(
      battery_saver_cannot_freeze_reasons.Has(CannotFreezeReason::kLoading));
  ukm.SetNotificationPermission(battery_saver_cannot_freeze_reasons.Has(
      CannotFreezeReason::kNotificationPermission));

  ukm.Record(ukm::UkmRecorder::Get());
}

base::TimeTicks FreezingPolicy::GenerateRandomPeriodicUnfreezePhase() const {
  return base::TimeTicks() +
         base::Milliseconds(base::RandInt(
             0, features::kInfiniteTabsFreezing_UnfreezeInterval.Get()
                    .InMilliseconds()));
}

void FreezingPolicy::CheckMemoryPressureForFreezing() {
#if BUILDFLAG(IS_WIN)
  base::SystemMemoryInfo info;
  if (!base::GetSystemMemoryInfo(&info)) {
    // Cannot get system memory info, do nothing.
    return;
  }

  // The moderate pressure threshold value is lifted from the default logic in
  // SystemMemoryPressureEvaluator. It was determined experimentally to ensure
  // sufficient responsiveness of the memory pressure subsystem with minimal
  // overhead.
  const int kPressureThresholdPercent =
      features::kInfiniteTabsFreezingOnMemoryPressurePercent.Get();

  base::ByteCount total = info.total;
  base::ByteCount avail = info.avail_phys;

  int available_percent = 0;
  if (total.is_positive()) {
    available_percent =
        static_cast<int>(avail.InBytesF() / total.InBytesF() * 100.0);
  }

  bool is_now_under_pressure = available_percent < kPressureThresholdPercent;

  // If the pressure state hasn't changed, there's nothing to do.
  if (is_now_under_pressure == is_under_memory_pressure_) {
    return;
  }

  // The state has changed. Update the flag and re-evaluate all pages.
  is_under_memory_pressure_ = is_now_under_pressure;
  UpdateAllPagesFrozenState();

#endif  // BUILDFLAG(IS_WIN)
}

void FreezingPolicy::UpdateAllPagesFrozenState() {
  const base::TimeTicks now = base::TimeTicks::Now();

  base::flat_set<raw_ptr<const PageNode>> visited_pages;
  for (auto& [id, state] : browsing_instance_states_) {
    if (!base::Contains(visited_pages, *state.pages.begin())) {
      UpdateFrozenState(*state.pages.begin(), now, &visited_pages);
    }
  }
}

}  // namespace performance_manager

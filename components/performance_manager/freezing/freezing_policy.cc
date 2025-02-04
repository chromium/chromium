// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_policy.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
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

using resource_attribution::OriginInBrowsingInstanceContext;

constexpr base::TimeDelta kCPUMeasurementInterval = base::Minutes(1);

struct PageFreezingState
    : public ExternalNodeAttachedDataImpl<PageFreezingState> {
  explicit PageFreezingState(const PageNodeImpl*) {}
  ~PageFreezingState() override = default;

  PageFreezingState(const PageFreezingState&) = delete;
  PageFreezingState& operator=(const PageFreezingState&) = delete;

  static PageFreezingState& FromPage(const PageNode* page_node) {
    return *PageFreezingState::GetOrCreate(PageNodeImpl::FromNode(page_node));
  }

  // Whether this page is frozen.
  bool frozen = false;

  // Number of votes to freeze the page.
  int num_freeze_votes = 0;

  // Reasons not to freeze the page.
  CannotFreezeReasonSet cannot_freeze_reasons;

  // Timer to remove `CannotFreezeReason::kRecentlyVisible`.
  base::OneShotTimer recently_visible_timer;

  // Timer to remove `CannotFreezeReason::kRecentlyAudible`.
  base::OneShotTimer recently_audible_timer;
};

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
}

FreezingPolicy::~FreezingPolicy() = default;

void FreezingPolicy::ToggleFreezingOnBatterySaverMode(bool is_enabled) {
  is_battery_saver_active_ = is_enabled;

  // Update frozen state for all connected sets of pages (toggling the state of
  // battery saver mode can affect the frozen state of any connected set).
  base::flat_set<raw_ptr<const PageNode>> visited_pages;
  for (auto& [id, state] : browsing_instance_states_) {
    if (!base::Contains(visited_pages, *state.pages.begin())) {
      UpdateFrozenState(*state.pages.begin(), &visited_pages);
    }
  }
}

void FreezingPolicy::AddFreezeVote(PageNode* page_node) {
  int prev_num_freeze_votes =
      PageFreezingState::FromPage(page_node).num_freeze_votes++;
  // A browsing instance may be frozen if there is at least one freeze vote.
  // Therefore, it's only necessary to update the frozen state when adding the
  // first freeze vote.
  if (prev_num_freeze_votes == 0) {
    UpdateFrozenState(page_node);
  }
}

void FreezingPolicy::RemoveFreezeVote(PageNode* page_node) {
  int num_freeze_votes =
      --PageFreezingState::FromPage(page_node).num_freeze_votes;
  // A browsing instance may be frozen if there is at least one freeze vote.
  // Therefore, it's only necessary to update the frozen state when removing the
  // last freeze vote.
  if (num_freeze_votes == 0) {
    UpdateFrozenState(page_node);
  }
}

std::set<std::string> FreezingPolicy::GetCannotFreezeReasons(
    const PageNode* page_node) {
  // Note: A set is used to de-duplicate `CannotFreezeReason`s added multiple
  // times when traversing connected pages.
  std::set<std::string> cannot_freeze_reasons;

  // `CannotFreezeReason`s for this page.
  const auto& page_freezing_state = PageFreezingState::FromPage(page_node);
  for (auto reason : page_freezing_state.cannot_freeze_reasons) {
    cannot_freeze_reasons.insert(CannotFreezeReasonToString(reason));
  }

  // `CannotFreezeReason`s for connected pages.
  for (const PageNode* connected_page_node : GetConnectedPages(page_node)) {
    if (connected_page_node != page_node) {
      auto& connected_page_freezing_state =
          PageFreezingState::FromPage(connected_page_node);
      for (auto reason : connected_page_freezing_state.cannot_freeze_reasons) {
        cannot_freeze_reasons.insert(base::StringPrintf(
            "%s (from connected page)", CannotFreezeReasonToString(reason)));
      }
    }
  }

  return cannot_freeze_reasons;
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

void FreezingPolicy::UpdateFrozenState(
    const PageNode* page,
    base::flat_set<raw_ptr<const PageNode>>* connected_pages_out) {
  const base::flat_set<raw_ptr<const PageNode>> connected_pages =
      GetConnectedPages(page);

  // Determine whether:
  // - Any connected page has a `CannotFreezeReason`.
  // - Any browsing instance hosting a frame from a connected page was CPU
  //   intensive in the background and Battery Saver is active and the
  //   `kFreezingOnBatterySaver` feature is enabled.
  // - All connected page have a freeze vote.
  bool has_cannot_freeze_reason = false;
  bool eligible_for_freezing_on_battery_saver = false;
  bool all_pages_have_freeze_vote = true;

  const double high_cpu_proportion = features::kFreezingHighCPUProportion.Get();

  for (const PageNode* visited_page : connected_pages) {
    auto& page_freezing_state = PageFreezingState::FromPage(visited_page);

    has_cannot_freeze_reason |=
        !page_freezing_state.cannot_freeze_reasons.empty();
    all_pages_have_freeze_vote &= (page_freezing_state.num_freeze_votes > 0);

    for (auto browsing_instance_id : GetBrowsingInstances(visited_page)) {
      auto it = browsing_instance_states_.find(browsing_instance_id);
      CHECK(it != browsing_instance_states_.end());
      const BrowsingInstanceState& browsing_instance_state = it->second;

      if (browsing_instance_state
                  .highest_cpu_any_interval_without_cannot_freeze_reason >=
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

  const bool should_be_frozen =
      !has_cannot_freeze_reason &&
      (eligible_for_freezing_on_battery_saver || all_pages_have_freeze_vote);

  // Freeze/unfreeze connected pages as needed.
  for (const PageNode* connected_page : connected_pages) {
    auto& page_freezing_state = PageFreezingState::FromPage(connected_page);
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
  auto& page_freezing_state = PageFreezingState::FromPage(page_node);
  if (add) {
    DCHECK(!page_freezing_state.cannot_freeze_reasons.Has(reason));
    const bool was_empty = page_freezing_state.cannot_freeze_reasons.empty();
    page_freezing_state.cannot_freeze_reasons.Put(reason);
    if (was_empty) {
      // Track that the browsing instance had a `CannotFreezeReason`, so that
      // the next CPU measurement for it can be ignored (this bit is sticky and
      // won't be reset if the `CannotFreezeReason` is removed before the next
      // measurement).
      for (auto browsing_instance_id : GetBrowsingInstances(page_node)) {
        auto it = browsing_instance_states_.find(browsing_instance_id);
        CHECK(it != browsing_instance_states_.end());
        it->second.cannot_freeze_reasons_since_last_cpu_measurement.Put(reason);
      }

      UpdateFrozenState(page_node);
    }
  } else {
    DCHECK(page_freezing_state.cannot_freeze_reasons.Has(reason));
    page_freezing_state.cannot_freeze_reasons.Remove(reason);
    if (page_freezing_state.cannot_freeze_reasons.empty()) {
      UpdateFrozenState(page_node);
    }
  }
}

//  static
CannotFreezeReasonSet FreezingPolicy::GetCannotFreezeReasons(
    const BrowsingInstanceState& browsing_instance_state) {
  CannotFreezeReasonSet reasons;
  for (const PageNode* page : browsing_instance_state.pages) {
    const auto& page_freezing_state = PageFreezingState::FromPage(page);
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
  auto& page_freezing_state = PageFreezingState::FromPage(page_node);

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
  CHECK(page_node->GetMainFrameNodes().empty());
}

void FreezingPolicy::OnIsVisibleChanged(const PageNode* page_node) {
  auto& page_freezing_state = PageFreezingState::FromPage(page_node);
  if (page_node->IsVisible()) {
    OnCannotFreezeReasonChange(page_node, /*add=*/true,
                               CannotFreezeReason::kVisible);
    if (page_freezing_state.recently_visible_timer.IsRunning()) {
      page_freezing_state.recently_visible_timer.Stop();
      OnCannotFreezeReasonChange(page_node, /*add=*/false,
                                 CannotFreezeReason::kRecentlyVisible);
    }
  } else {
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
}

void FreezingPolicy::OnIsAudibleChanged(const PageNode* page_node) {
  auto& page_freezing_state = PageFreezingState::FromPage(page_node);
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
      it->second.per_origin_pmf_after_freezing_kb.clear();
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
      PageFreezingState::FromPage(page_node).cannot_freeze_reasons.Has(
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

  const auto& page_freezing_state = PageFreezingState::FromPage(node);

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
              PageFreezingState::FromPage(other_page_node)
                  .cannot_freeze_reasons);
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
      if (state.per_origin_pmf_after_freezing_kb.empty()) {
        browsing_instance_states_without_initial_measurement.insert(id);
      }
    } else {
      // Should have been cleared by `OnPageLifecycleStateChanged`.
      CHECK(state.per_origin_pmf_after_freezing_kb.empty());
    }
  }

  const int growth_threshold_kb =
      features::kFreezingMemoryGrowthThresholdToDiscardKb.Get();

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

    const uint64_t current_kb =
        result.memory_summary_result->private_footprint_kb;
    if (base::Contains(browsing_instance_states_without_initial_measurement,
                       id)) {
      // Store the first PMF measurement after being frozen.
      state.per_origin_pmf_after_freezing_kb[origin_in_browsing_instance_context
                                                 .GetOrigin()] = current_kb;
    } else {
      // Compare current measurement against the one stored after being frozen.
      auto it = state.per_origin_pmf_after_freezing_kb.find(
          origin_in_browsing_instance_context.GetOrigin());
      uint64_t after_freezing_kb;
      if (it == state.per_origin_pmf_after_freezing_kb.end()) {
        // No memory measurement was stored for this origin after being frozen.
        // This could indicate a measurement error (e.g. process missing in a
        // `memory_instrumentation::GlobalMemoryDump`) or that the browsing
        // instance navigated while frozen (e.g. requested by an extension).
        //
        // Pretend that 0 was stored after freezing. This will cause the
        // browsing instance to be discarded iff the current measurement is
        // above the growth threshold. In any case, no extra measurement is
        // stored in `per_origin_pmf_after_freezing_kb` to prevent it from
        // growing without bounds if the page continuously navigates to new
        // origins.
        after_freezing_kb = 0;
      } else {
        after_freezing_kb = it->second;
      }

      const uint64_t growth_kb =
          current_kb > after_freezing_kb ? current_kb - after_freezing_kb : 0u;
      if (growth_kb > base::checked_cast<uint64_t>(growth_threshold_kb)) {
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

    if (!state.cannot_freeze_reasons_since_last_cpu_measurement.empty()) {
      // Ignore CPU measurement while having a `CannotFreezeReason` (it's
      // acceptable to use a lot of CPU while playing audio, running a
      // videoconference call...).
      continue;
    }

    if (state.highest_cpu_any_interval_without_cannot_freeze_reason >
        cpu_proportion) {
      // Ignore CPU measurement without a `CannotFreezeReason` if it's not the
      // highest one.
      continue;
    }

    // Store the new highest CPU measurement without a `CannotFreezeReason`.
    state.highest_cpu_any_interval_without_cannot_freeze_reason =
        cpu_proportion;

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
        PageFreezingState::FromPage(page_node).cannot_freeze_reasons.Has(
            CannotFreezeReason::kOptedOut);
    const bool is_opted_out = opt_out_checker_->IsPageOptedOutOfFreezing(
        browser_context_id, page_node->GetMainFrameUrl());
    if (was_opted_out != is_opted_out) {
      OnCannotFreezeReasonChange(page_node, /*add=*/is_opted_out,
                                 CannotFreezeReason::kOptedOut);
    }
  }
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

  for (auto* page : GetOwningGraph()->GetAllPageNodes()) {
    if (visited_pages.contains(page)) {
      // The page is part of a group of connected pages for which the UKM event
      // was already emitted.
      continue;
    }

    std::optional<double> highest_cpu_current_interval;
    double highest_cpu_any_interval_without_cannot_freeze_reason = 0.0;
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

        highest_cpu_any_interval_without_cannot_freeze_reason = std::max(
            highest_cpu_any_interval_without_cannot_freeze_reason,
            state.highest_cpu_any_interval_without_cannot_freeze_reason);
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
            highest_cpu_any_interval_without_cannot_freeze_reason,
            cannot_freeze_reasons);
      }
    }

    visited_pages.insert(connected_pages.begin(), connected_pages.end());
  }
}

void FreezingPolicy::RecordFreezingEligibilityUKMForPage(
    ukm::SourceId source_id,
    double highest_cpu_current_interval,
    double highest_cpu_any_interval_without_cannot_freeze_reason,
    CannotFreezeReasonSet cannot_freeze_reasons) {
  RecordFreezingEligibilityUKMForPageStatic(
      source_id, highest_cpu_current_interval,
      highest_cpu_any_interval_without_cannot_freeze_reason,
      cannot_freeze_reasons);
}

void FreezingPolicy::RecordFreezingEligibilityUKMForPageStatic(
    ukm::SourceId source_id,
    double highest_cpu_current_interval,
    double highest_cpu_any_interval_without_cannot_freeze_reason,
    CannotFreezeReasonSet cannot_freeze_reasons) {
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
          highest_cpu_any_interval_without_cannot_freeze_reason * 100));

  ukm.SetVisible(cannot_freeze_reasons.Has(CannotFreezeReason::kVisible));
  ukm.SetRecentlyVisible(
      cannot_freeze_reasons.Has(CannotFreezeReason::kRecentlyVisible));
  ukm.SetAudible(cannot_freeze_reasons.Has(CannotFreezeReason::kAudible));
  ukm.SetRecentlyAudible(
      cannot_freeze_reasons.Has(CannotFreezeReason::kRecentlyAudible));
  ukm.SetOriginTrialOptOut(cannot_freeze_reasons.Has(
      CannotFreezeReason::kFreezingOriginTrialOptOut));
  ukm.SetHoldingWebLock(
      cannot_freeze_reasons.Has(CannotFreezeReason::kHoldingWebLock));
  ukm.SetHoldingBlockingIndexedDBLock(cannot_freeze_reasons.Has(
      CannotFreezeReason::kHoldingBlockingIndexedDBLock));
  ukm.SetConnectedToDevice(cannot_freeze_reasons.HasAny(
      {CannotFreezeReason::kConnectedToUsbDevice,
       CannotFreezeReason::kConnectedToBluetoothDevice,
       CannotFreezeReason::kConnectedToHidDevice,
       CannotFreezeReason::kConnectedToSerialPort}));
  ukm.SetCapturing(cannot_freeze_reasons.HasAny(
      {CannotFreezeReason::kCapturingAudio, CannotFreezeReason::kCapturingVideo,
       CannotFreezeReason::kCapturingWindow,
       CannotFreezeReason::kCapturingDisplay}));
  ukm.SetBeingMirrored(
      cannot_freeze_reasons.Has(CannotFreezeReason::kBeingMirrored));
  ukm.SetWebRTC(cannot_freeze_reasons.Has(CannotFreezeReason::kWebRTC));
  ukm.SetLoading(cannot_freeze_reasons.Has(CannotFreezeReason::kLoading));
  ukm.SetNotificationPermission(
      cannot_freeze_reasons.Has(CannotFreezeReason::kNotificationPermission));

  ukm.Record(ukm::UkmRecorder::Get());
}

}  // namespace performance_manager

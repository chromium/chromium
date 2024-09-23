// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/freezing/freezing_policy.h"

#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/timer/timer.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/resource_attribution/origin_in_browsing_instance_context.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"

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
  std::vector<CannotFreezeReason> cannot_freeze_reasons;

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

FreezingPolicy::FreezingPolicy()
    : freezer_(std::make_unique<Freezer>()),
      cpu_proportion_tracker_(
          /*context_filter=*/base::NullCallback(),
          /*cpu_proportion_type=*/resource_attribution::CPUProportionTracker::
              CPUProportionType::kBackground) {
  if (base::FeatureList::IsEnabled(features::kCPUMeasurementInFreezingPolicy)) {
    cpu_usage_query_ =
        resource_attribution::QueryBuilder()
            .AddAllContextsOfType<OriginInBrowsingInstanceContext>()
            .AddResourceType(resource_attribution::ResourceType::kCPUTime)
            .CreateScopedQuery();
    cpu_usage_query_observation_.Observe(&cpu_usage_query_.value());
    cpu_usage_query_->Start(kCPUMeasurementInterval);
  }
}

FreezingPolicy::~FreezingPolicy() = default;

void FreezingPolicy::ToggleFreezingOnBatterySaverMode(bool is_enabled) {
  is_battery_saver_active_ = is_enabled;

  // Update frozen state for all connected sets of pages (toggling the state of
  // battery saver mode can affect the frozen state of any connected set).
  base::flat_set<raw_ptr<const PageNode>> visited_pages;
  for (auto& [id, state] : browsing_instances_) {
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

FreezingPolicy::BrowsingInstanceState::BrowsingInstanceState() = default;
FreezingPolicy::BrowsingInstanceState::~BrowsingInstanceState() = default;

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
  // Build a set of pages connected to `page` and determine whether:
  // - Any connected page has a `CannotFreezeReason`.
  // - Any browsing instance hosting a frame from a connected page was CPU
  //   intensive in the background and Battery Saver is active and the
  //   `kFreezingOnBatterySaver` feature is enabled.
  // - All connected page have a freeze vote.
  base::flat_set<raw_ptr<const PageNode>> connected_pages;
  bool has_cannot_freeze_reason = false;
  bool eligible_for_freezing_on_battery_saver = false;
  bool all_pages_have_freeze_vote = true;

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

    auto& page_freezing_state = PageFreezingState::FromPage(visited_page);

    has_cannot_freeze_reason |=
        !page_freezing_state.cannot_freeze_reasons.empty();
    all_pages_have_freeze_vote &= (page_freezing_state.num_freeze_votes > 0);

    for (auto browsing_instance_id : GetBrowsingInstances(visited_page)) {
      auto it = browsing_instances_.find(browsing_instance_id);
      CHECK(it != browsing_instances_.end());
      const BrowsingInstanceState& browsing_instance_state = it->second;

      if (browsing_instance_state.cpu_intensive_in_background &&
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

      for (auto* browsing_instance_page : it->second.pages) {
        if (!base::Contains(connected_pages, browsing_instance_page)) {
          pages_to_visit.insert(browsing_instance_page);
        }
      }
    }
  }

  bool should_be_frozen =
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
    DCHECK(!base::Contains(page_freezing_state.cannot_freeze_reasons, reason));
    page_freezing_state.cannot_freeze_reasons.push_back(reason);
    if (page_freezing_state.cannot_freeze_reasons.size() == 1U) {
      // Track that the browsing instance had a `CannotFreezeReason`, so that
      // the next CPU measurement for it can be ignored (this bit is sticky and
      // won't be reset if the `CannotFreezeReason` is removed before the next
      // measurement).
      for (auto browsing_instance_id : GetBrowsingInstances(page_node)) {
        auto it = browsing_instances_.find(browsing_instance_id);
        CHECK(it != browsing_instances_.end());
        it->second.had_cannot_freeze_reason_since_last_cpu_measurement = true;
      }

      UpdateFrozenState(page_node);
    }
  } else {
    size_t num_removed =
        std::erase(page_freezing_state.cannot_freeze_reasons, reason);
    CHECK_EQ(num_removed, 1U);
    if (page_freezing_state.cannot_freeze_reasons.empty()) {
      UpdateFrozenState(page_node);
    }
  }
}

//  static
bool FreezingPolicy::HasCannotFreezeReason(
    const BrowsingInstanceState& browsing_instance_state) {
  for (const PageNode* page : browsing_instance_state.pages) {
    const auto& page_freezing_state = PageFreezingState::FromPage(page);
    if (!page_freezing_state.cannot_freeze_reasons.empty()) {
      return true;
    }
  }
  return false;
}

void FreezingPolicy::OnPassedToGraph(Graph* graph) {
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this, "Freezing");
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
    page_freezing_state.cannot_freeze_reasons.push_back(
        CannotFreezeReason::kVisible);
  }

  if (page_node->IsAudible()) {
    page_freezing_state.cannot_freeze_reasons.push_back(
        CannotFreezeReason::kAudible);
  }

  DCHECK(!page_node->IsHoldingWebLock());
  DCHECK(!page_node->IsHoldingIndexedDBLock());
  DCHECK(!IsPageConnectedToUSBDevice(page_node));
  DCHECK(!IsPageConnectedToBluetoothDevice(page_node));
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

void FreezingPolicy::OnPageIsHoldingWebLockChanged(const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node, /*add=*/page_node->IsHoldingWebLock(),
                             CannotFreezeReason::kHoldingWebLock);
}

void FreezingPolicy::OnPageUsesWebRTCChanged(const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node, /*add=*/page_node->UsesWebRTC(),
                             CannotFreezeReason::kWebRTC);
}

void FreezingPolicy::OnPageIsHoldingIndexedDBLockChanged(
    const PageNode* page_node) {
  OnCannotFreezeReasonChange(page_node,
                             /*add=*/page_node->IsHoldingIndexedDBLock(),
                             CannotFreezeReason::kHoldingIndexedDBLock);
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
      browsing_instances_[frame_node->GetBrowsingInstanceId()];
  auto [it, inserted] =
      browsing_instance_state.pages.insert(frame_node->GetPageNode());
  if (!inserted) {
    return;
  }

  // Update frozen state for browsing instances associated with the frame's
  // page.
  UpdateFrozenState(frame_node->GetPageNode());
}

void FreezingPolicy::OnBeforeFrameNodeRemoved(const FrameNode* frame_node) {
  if (!frame_node->IsMainFrame()) {
    return;
  }

  // Early exit if another main frame is associated with the same browsing
  // instance (in other words, the set of browsing instances associated with the
  // removed frame's page doesn't change).
  for (const FrameNode* other_frame_node :
       frame_node->GetPageNode()->GetMainFrameNodes()) {
    if (other_frame_node != frame_node &&
        other_frame_node->GetBrowsingInstanceId() ==
            frame_node->GetBrowsingInstanceId()) {
      return;
    }
  }

  // Disassociate the frame's page from the frame's browsing instance.
  auto it = browsing_instances_.find(frame_node->GetBrowsingInstanceId());
  CHECK(it != browsing_instances_.end());
  size_t num_pages_removed = it->second.pages.erase(frame_node->GetPageNode());
  CHECK_EQ(num_pages_removed, 1U);

  // Update frozen state for pages connected to the frame's page, if it still
  // contains at least one frame.
  if (frame_node->GetPageNode()->GetMainFrameNode()) {
    UpdateFrozenState(frame_node->GetPageNode());
  }

  // If pages remain in the deleted frame's browsing instance, update their
  // frozen state (note: these pages may no longer be connected to the frame's
  // page). Otherwise, delete the deleted frame's browsing instance state.
  if (it->second.pages.empty()) {
    browsing_instances_.erase(it);
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
    base::flat_set<CannotFreezeReason> cannot_freeze_reasons_other_pages;
    for (auto browsing_instance : GetBrowsingInstances(node)) {
      auto browsing_instance_it = browsing_instances_.find(browsing_instance);
      CHECK(browsing_instance_it != browsing_instances_.end());
      for (const PageNode* other_page_node :
           browsing_instance_it->second.pages) {
        if (other_page_node == node) {
          continue;
        }

        auto& other_page_freezing_state =
            PageFreezingState::FromPage(other_page_node);
        for (auto reason : other_page_freezing_state.cannot_freeze_reasons) {
          cannot_freeze_reasons_other_pages.insert(reason);
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
  if (!cpu_proportion_tracker_.IsTracking()) {
    cpu_proportion_tracker_.StartFirstInterval(base::TimeTicks::Now(), results);
    return;
  }

  const double high_cpu_proportion = features::kFreezingHighCPUProportion.Get();
  const std::map<resource_attribution::ResourceContext, double>
      cpu_proportion_map = cpu_proportion_tracker_.StartNextInterval(
          base::TimeTicks::Now(), results);
  for (const auto& [context, cpu_proportion] : cpu_proportion_map) {
    if (cpu_proportion < high_cpu_proportion) {
      continue;
    }

    // This cast is valid because the query only targets contexts of type
    // `OriginInBrowsingInstanceContext` (verified by CHECK inside `AsContext`).
    const auto& origin_in_browsing_instance_context =
        resource_attribution::AsContext<OriginInBrowsingInstanceContext>(
            context);
    auto browsing_instance_it = browsing_instances_.find(
        origin_in_browsing_instance_context.GetBrowsingInstance());
    if (browsing_instance_it == browsing_instances_.end()) {
      continue;
    }

    if (browsing_instance_it->second.cpu_intensive_in_background) {
      // Already known to be CPU-intensive in background.
      continue;
    }

    if (browsing_instance_it->second
            .had_cannot_freeze_reason_since_last_cpu_measurement) {
      // CPU-intensive in background while having a `CannotFreezeReason` isn't
      // recorded (it's acceptable to use a lot of CPU while playing audio,
      // running a videoconference call...).
      continue;
    }

    browsing_instance_it->second.cpu_intensive_in_background = true;
    UpdateFrozenState(*browsing_instance_it->second.pages.begin());
  }

  // Update `had_cannot_freeze_reason_since_last_cpu_measurement` for all
  // browsing instances.
  for (auto& [_, browsing_instance_state] : browsing_instances_) {
    browsing_instance_state
        .had_cannot_freeze_reason_since_last_cpu_measurement =
        HasCannotFreezeReason(browsing_instance_state);
  }
}

}  // namespace performance_manager

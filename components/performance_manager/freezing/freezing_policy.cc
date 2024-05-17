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
#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/resource_attribution/origin_in_browsing_instance_context.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"

namespace performance_manager {

namespace {

using resource_attribution::OriginInBrowsingInstanceContext;

constexpr base::TimeDelta kCPUMeasurementInterval = base::Minutes(1);

struct PageFreezingState : public NodeAttachedDataImpl<PageFreezingState> {
  struct Traits : public NodeAttachedDataInMap<PageNodeImpl> {};

  explicit PageFreezingState(const PageNodeImpl*) {}
  ~PageFreezingState() override = default;

  PageFreezingState(const PageFreezingState&) = delete;
  PageFreezingState& operator=(const PageFreezingState&) = delete;

  static PageFreezingState& FromPage(const PageNode* page_node) {
    return *PageFreezingState::GetOrCreate(PageNodeImpl::FromNode(page_node));
  }

  // Number of votes to freeze the page.
  int num_freeze_votes = 0;

  // Reasons not to freeze the page.
  std::vector<CannotFreezeReason> cannot_freeze_reasons;

  // Timer to remove the `CannotFreezeReason::kRecentlyAudible`.
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

  for (auto& [_, browsing_instance_state] : browsing_instances_) {
    UpdateFrozenState(browsing_instance_state);
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
  for (auto& frame_node : page->GetMainFrameNodes()) {
    browsing_instances.insert(frame_node->GetBrowsingInstanceId());
  }
  return browsing_instances;
}

void FreezingPolicy::UpdateFrozenState(
    BrowsingInstanceState& browsing_instance_state) {
  const bool was_frozen = browsing_instance_state.frozen;

  bool should_be_frozen;
  if (HasCannotFreezeReason(browsing_instance_state)) {
    // Cannot be frozen if a page has a `CannotFreezeReason`.
    should_be_frozen = false;
    browsing_instance_state
        .had_cannot_freeze_reason_since_last_cpu_measurement = true;
  } else if (is_battery_saver_active_ &&
             browsing_instance_state.cpu_intensive_in_background &&
             // Note: Feature state is checked last so that only clients that
             // have a browsing instance that is CPU intensive in background
             // while Battery Saver is active are enrolled in the experiment.
             base::FeatureList::IsEnabled(features::kFreezingOnBatterySaver)) {
    // Should be frozen if Battery Saver is active and the browsing instance was
    // CPU-intensive in background.
    should_be_frozen = true;
  } else {
    // Should be frozen if all pages have a freeze vote.
    should_be_frozen = true;
    for (const PageNode* page : browsing_instance_state.pages) {
      auto& page_freezing_state = PageFreezingState::FromPage(page);
      if (page_freezing_state.num_freeze_votes == 0) {
        should_be_frozen = false;
        break;
      }
    }
  }

  if (was_frozen == should_be_frozen) {
    return;
  }

  browsing_instance_state.frozen = should_be_frozen;

  // Apply frozen state change to pages in the browsing instance.
  if (should_be_frozen) {
    for (const PageNode* page : browsing_instance_state.pages) {
      freezer_->MaybeFreezePageNode(page);
    }
  } else {
    for (const PageNode* page : browsing_instance_state.pages) {
      freezer_->UnfreezePageNode(page);
    }
  }
}

void FreezingPolicy::UpdateFrozenState(const PageNode* page_node) {
  for (content::BrowsingInstanceId browsing_instance :
       GetBrowsingInstances(page_node)) {
    auto it = browsing_instances_.find(browsing_instance);
    CHECK(it != browsing_instances_.end());
    UpdateFrozenState(it->second);
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
  graph->RegisterObject(this);
  graph->AddPageNodeObserver(this);
  graph->AddFrameNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this, "Freezing");
}

void FreezingPolicy::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->UnregisterObject(this);
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
  OnCannotFreezeReasonChange(page_node, /*add=*/page_node->IsVisible(),
                             CannotFreezeReason::kVisible);
}

void FreezingPolicy::OnIsAudibleChanged(const PageNode* page_node) {
  if (page_node->IsAudible()) {
    OnCannotFreezeReasonChange(page_node, /*add=*/true,
                               CannotFreezeReason::kAudible);
    auto& page_freezing_state = PageFreezingState::FromPage(page_node);
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

    PageFreezingState::FromPage(page_node).recently_audible_timer.Start(
        FROM_HERE, kAudioProtectionTime,
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

  // Add `CannotFreezeReason::kManyBrowsingInstances` if the frame's page
  // becomes associated with more than one browsing instance. As of April 2024,
  // this may happen momentarily when there are inactive (an inactive frame in
  // BFCache or prerendering may belong to a different browsing instance than
  // the corresponding active frame) or when there are fenced frames (a fenced
  // frame is in a different browsing instance than its parent).
  if (GetBrowsingInstances(frame_node->GetPageNode()).size() > 1U) {
    auto& page_freezing_state =
        PageFreezingState::FromPage(frame_node->GetPageNode());
    if (!base::Contains(page_freezing_state.cannot_freeze_reasons,
                        CannotFreezeReason::kManyBrowsingInstances)) {
      page_freezing_state.cannot_freeze_reasons.push_back(
          CannotFreezeReason::kManyBrowsingInstances);
    }
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
  base::flat_set<content::BrowsingInstanceId> other_browsing_instance_ids;
  for (auto& other_frame_node :
       frame_node->GetPageNode()->GetMainFrameNodes()) {
    if (other_frame_node != frame_node) {
      if (other_frame_node->GetBrowsingInstanceId() ==
          frame_node->GetBrowsingInstanceId()) {
        return;
      }
      other_browsing_instance_ids.insert(
          other_frame_node->GetBrowsingInstanceId());
    }
  }

  // Disassociate the frame's page from the frame's browsing instance.
  auto it = browsing_instances_.find(frame_node->GetBrowsingInstanceId());
  size_t num_browsing_instances_removed =
      it->second.pages.erase(frame_node->GetPageNode());
  CHECK_EQ(num_browsing_instances_removed, 1U);

  // Remove `CannotFreezeReason::kManyBrowsingInstances` if the frame's page is
  // no longer associated with more than one browsing instance.
  if (other_browsing_instance_ids.size() <= 1U) {
    auto& page_freezing_state =
        PageFreezingState::FromPage(frame_node->GetPageNode());
    size_t num_cannot_freeze_reason_removed =
        std::erase(page_freezing_state.cannot_freeze_reasons,
                   CannotFreezeReason::kManyBrowsingInstances);
    CHECK_LE(num_cannot_freeze_reason_removed, 1U);
  }

  // Update frozen state for browsing instances associated with the frame's page
  // (including the disassociated browsing instance).
  UpdateFrozenState(frame_node->GetPageNode());

  // Delete the disassociated browsing instance's state if empty.
  if (it->second.pages.empty()) {
    browsing_instances_.erase(it);
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

  const double high_cpu_proportion =
      features::kFreezingOnBatterySaverHighCPUProportion.Get();
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
    UpdateFrozenState(browsing_instance_it->second);
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

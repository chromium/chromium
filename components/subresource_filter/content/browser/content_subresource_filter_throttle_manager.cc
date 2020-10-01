// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/subresource_filter/content/browser/activation_state_computing_navigation_throttle.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/page_load_statistics.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/content/common/subresource_filter_utils.h"
#include "components/subresource_filter/content/mojom/subresource_filter_agent.mojom.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace subresource_filter {

ContentSubresourceFilterThrottleManager::
    ContentSubresourceFilterThrottleManager(
        SubresourceFilterClient* client,
        VerifiedRulesetDealer::Handle* dealer_handle,
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      receiver_(web_contents, this),
      dealer_handle_(dealer_handle),
      client_(client) {
  SubresourceFilterObserverManager::CreateForWebContents(web_contents);
  scoped_observer_.Add(
      SubresourceFilterObserverManager::FromWebContents(web_contents));
}

ContentSubresourceFilterThrottleManager::
    ~ContentSubresourceFilterThrottleManager() {}

void ContentSubresourceFilterThrottleManager::OnSubresourceFilterGoingAway() {
  // Stop observing here because the observer manager could be destroyed by the
  // time this class is destroyed.
  scoped_observer_.RemoveAll();
}

void ContentSubresourceFilterThrottleManager::RenderFrameDeleted(
    content::RenderFrameHost* frame_host) {
  frame_host_filter_map_.erase(frame_host);
  navigated_frames_.erase(frame_host);
  ad_frames_.erase(frame_host);
  navigation_load_policies_.erase(frame_host);
  DestroyRulesetHandleIfNoLongerUsed();
}

// Pull the AsyncDocumentSubresourceFilter and its associated
// mojom::ActivationState out of the activation state computing throttle. Store
// it for later filtering of subframe navigations.
void ContentSubresourceFilterThrottleManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // Since the frame hasn't yet committed, GetCurrentRenderFrameHost() points
  // to the initial RFH.
  // TODO(crbug.com/843646): Use an API that NavigationHandle supports rather
  // than trying to infer what the NavigationHandle is doing.
  content::RenderFrameHost* previous_rfh =
      navigation_handle->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
          navigation_handle->GetFrameTreeNodeId());

  // If a known ad RenderFrameHost has moved to a new host, update |ad_frames_|.
  bool transferred_ad_frame = false;
  if (previous_rfh && previous_rfh != navigation_handle->GetRenderFrameHost()) {
    auto previous_rfh_it = ad_frames_.find(previous_rfh);
    if (previous_rfh_it != ad_frames_.end()) {
      ad_frames_.erase(previous_rfh_it);
      ad_frames_.insert(navigation_handle->GetRenderFrameHost());
      transferred_ad_frame = true;
    }

    // If |previous_rfh| exists and is different than the final render frame
    // host for the navigation, we need to associate the load policy with the
    // new rfh.
    auto navigation_load_policy_it =
        navigation_load_policies_.find(previous_rfh);
    if (navigation_load_policy_it != navigation_load_policies_.end()) {
      navigation_load_policies_.emplace(navigation_handle->GetRenderFrameHost(),
                                        navigation_load_policy_it->second);
      navigation_load_policies_.erase(navigation_load_policy_it);
    }
  }

  if (navigation_handle->GetNetErrorCode() != net::OK)
    return;

  auto it = ongoing_activation_throttles_.find(navigation_handle);
  if (it == ongoing_activation_throttles_.end())
    return;

  // Main frame throttles with disabled page-level activation will not have
  // associated filters.
  ActivationStateComputingNavigationThrottle* throttle = it->second;
  AsyncDocumentSubresourceFilter* filter = throttle->filter();
  if (!filter)
    return;

  // A filter with DISABLED activation indicates a corrupted ruleset.
  mojom::ActivationLevel level = filter->activation_state().activation_level;
  if (level == mojom::ActivationLevel::kDisabled)
    return;

  TRACE_EVENT2(
      TRACE_DISABLED_BY_DEFAULT("loading"),
      "ContentSubresourceFilterThrottleManager::ReadyToCommitNavigation",
      "activation_state",
      static_cast<int>(filter->activation_state().activation_level),
      "render_frame_host",
      base::trace_event::ToTracedValue(
          navigation_handle->GetRenderFrameHost()));

  throttle->WillSendActivationToRenderer();

  content::RenderFrameHost* frame_host =
      navigation_handle->GetRenderFrameHost();

  bool is_ad_subframe =
      transferred_ad_frame || base::Contains(ad_frames_, frame_host);
  DCHECK(!is_ad_subframe || !navigation_handle->IsInMainFrame());

  bool parent_is_ad = base::Contains(ad_frames_, frame_host->GetParent());

  blink::mojom::AdFrameType ad_frame_type = blink::mojom::AdFrameType::kNonAd;
  if (is_ad_subframe) {
    ad_frame_type = parent_is_ad ? blink::mojom::AdFrameType::kChildAd
                                 : blink::mojom::AdFrameType::kRootAd;
    // Replicate ad frame type to this frame's proxies, so that it can be looked
    // up in any process involved in rendering the current page.
    frame_host->UpdateAdFrameType(ad_frame_type);
  }

  mojo::AssociatedRemote<mojom::SubresourceFilterAgent> agent;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
  agent->ActivateForNextCommittedLoad(filter->activation_state().Clone(),
                                      ad_frame_type);
}

void ContentSubresourceFilterThrottleManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Make sure not to leak throttle pointers.
  ActivationStateComputingNavigationThrottle* throttle = nullptr;
  auto throttle_it = ongoing_activation_throttles_.find(navigation_handle);
  if (throttle_it != ongoing_activation_throttles_.end()) {
    throttle = throttle_it->second;
    ongoing_activation_throttles_.erase(throttle_it);
  }

  // Do nothing if the navigation finished in the same document.
  if (navigation_handle->IsSameDocument()) {
    return;
  }
  if (!navigation_handle->HasCommitted()) {
    // TODO(crbug.com/1055558): Handle the case of an aborted main frame load.

    // If the initial load was aborted, the frame's activation will never have
    // been set and should instead be inherited from its parents. Reuse the
    // previous activation in the case of a non-initial aborted load.
    if (!navigation_handle->IsInMainFrame() &&
        navigation_handle->GetNetErrorCode() == net::ERR_ABORTED) {
      // Cannot get the RFH from navigation_handle due to the aborted load.
      content::RenderFrameHost* frame_host =
          navigation_handle->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
              navigation_handle->GetFrameTreeNodeId());

      // The RenderFrameHost will still exist as, even if a frame is destroyed,
      // the NavigationHandle is destroyed (resulting in a call to
      // DidFinishNavigation) before the RenderFrameHost is.
      DCHECK(frame_host);
      if (navigated_frames_.insert(frame_host).second) {
        DCHECK(!base::Contains(frame_host_filter_map_, frame_host));
        frame_host_filter_map_[frame_host] = nullptr;
      }
    }
    return;
  }

  std::unique_ptr<AsyncDocumentSubresourceFilter> filter;
  if (throttle) {
    CHECK_EQ(navigation_handle, throttle->navigation_handle());
    filter = throttle->ReleaseFilter();
  }

  content::RenderFrameHost* frame_host =
      navigation_handle->GetRenderFrameHost();
  navigated_frames_.insert(frame_host);

  if (navigation_handle->IsInMainFrame()) {
    current_committed_load_has_notified_disallowed_load_ = false;
    statistics_.reset();
    if (filter) {
      statistics_ =
          std::make_unique<PageLoadStatistics>(filter->activation_state());
      if (filter->activation_state().enable_logging) {
        DCHECK(filter->activation_state().activation_level !=
               mojom::ActivationLevel::kDisabled);
        frame_host->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kWarning,
            kActivationConsoleMessage);
      }
    }
    mojom::ActivationLevel level =
        filter ? filter->activation_state().activation_level
               : mojom::ActivationLevel::kDisabled;
    UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.PageLoad.ActivationState",
                              level);
  }

  // Make sure |frame_host_filter_map_| is updated or cleaned up depending on
  // this navigation's activation state.
  if (filter) {
    base::OnceClosure disallowed_callback(base::BindOnce(
        &ContentSubresourceFilterThrottleManager::MaybeShowNotification,
        weak_ptr_factory_.GetWeakPtr()));
    filter->set_first_disallowed_load_callback(std::move(disallowed_callback));
    frame_host_filter_map_[frame_host] = std::move(filter);
  } else {
    frame_host_filter_map_.erase(frame_host);

    // If this is for a special url that did not go through the navigation
    // throttles, then based on the parent's activation state, possibly add this
    // to frame_host_filter_map_.
    MaybeActivateSubframeSpecialUrls(navigation_handle);
  }

  DestroyRulesetHandleIfNoLongerUsed();
}

void ContentSubresourceFilterThrottleManager::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!statistics_ || render_frame_host->GetParent())
    return;
  statistics_->OnDidFinishLoad();
}

// Sets the desired page-level |activation_state| for the currently ongoing
// page load, identified by its main-frame |navigation_handle|. If this method
// is not called for a main-frame navigation, the default behavior is no
// activation for that page load.
void ContentSubresourceFilterThrottleManager::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const mojom::ActivationState& activation_state) {
  DCHECK(navigation_handle->IsInMainFrame());
  DCHECK(!navigation_handle->HasCommitted());

  auto it = ongoing_activation_throttles_.find(navigation_handle);
  if (it == ongoing_activation_throttles_.end())
    return;

  // The subresource filter normally operates in DryRun mode, disabled
  // activation should only be supplied in cases where DryRun mode is not
  // otherwise preferable. If the activation level is disabled, we do not want
  // to run any portion of the subresource filter on this navigation/frame. By
  // deleting the activation throttle, we prevent an associated
  // DocumentSubresourceFilter from being created at commit time. This
  // intentionally disables AdTagging and all dependent features for this
  // navigation/frame.
  if (activation_state.activation_level == mojom::ActivationLevel::kDisabled) {
    ongoing_activation_throttles_.erase(it);
    return;
  }

  it->second->NotifyPageActivationWithRuleset(EnsureRulesetHandle(),
                                              activation_state);
}

void ContentSubresourceFilterThrottleManager::OnSubframeNavigationEvaluated(
    content::NavigationHandle* navigation_handle,
    LoadPolicy load_policy,
    bool is_ad_subframe) {
  DCHECK(!navigation_handle->IsInMainFrame());

  // TODO(crbug.com/843646): Use an API that NavigationHandle supports rather
  // than trying to infer what the NavigationHandle is doing.
  content::RenderFrameHost* starting_rfh =
      navigation_handle->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
          navigation_handle->GetFrameTreeNodeId());
  DCHECK(starting_rfh);

  navigation_load_policies_[starting_rfh] = load_policy;

  if (is_ad_subframe)
    ad_frames_.insert(starting_rfh);
}

void ContentSubresourceFilterThrottleManager::MaybeAppendNavigationThrottles(
    content::NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  DCHECK(!navigation_handle->IsSameDocument());

  if (navigation_handle->IsInMainFrame() &&
      client_->GetSafeBrowsingDatabaseManager()) {
    throttles->push_back(
        std::make_unique<SubresourceFilterSafeBrowsingActivationThrottle>(
            navigation_handle, client_, content::GetIOThreadTaskRunner({}),
            client_->GetSafeBrowsingDatabaseManager()));
  }

  if (!dealer_handle_)
    return;
  if (auto filtering_throttle =
          MaybeCreateSubframeNavigationFilteringThrottle(navigation_handle)) {
    throttles->push_back(std::move(filtering_throttle));
  }

  DCHECK(!base::Contains(ongoing_activation_throttles_, navigation_handle));
  if (auto activation_throttle =
          MaybeCreateActivationStateComputingThrottle(navigation_handle)) {
    ongoing_activation_throttles_[navigation_handle] =
        activation_throttle.get();
    throttles->push_back(std::move(activation_throttle));
  }
}

bool ContentSubresourceFilterThrottleManager::CalculateIsAdSubframe(
    content::RenderFrameHost* frame_host,
    LoadPolicy load_policy) {
  DCHECK(frame_host);
  content::RenderFrameHost* parent_frame = frame_host->GetParent();
  DCHECK(parent_frame);

  return (load_policy != LoadPolicy::ALLOW &&
          load_policy != LoadPolicy::EXPLICITLY_ALLOW) ||
         base::Contains(ad_frames_, frame_host) ||
         base::Contains(ad_frames_, parent_frame);
}

bool ContentSubresourceFilterThrottleManager::IsFrameTaggedAsAd(
    const content::RenderFrameHost* frame_host) const {
  return base::Contains(ad_frames_, frame_host);
}

base::Optional<LoadPolicy>
ContentSubresourceFilterThrottleManager::LoadPolicyForLastCommittedNavigation(
    const content::RenderFrameHost* frame_host) const {
  auto it = navigation_load_policies_.find(frame_host);
  if (it != navigation_load_policies_.end())
    return it->second;
  return base::nullopt;
}

std::unique_ptr<SubframeNavigationFilteringThrottle>
ContentSubresourceFilterThrottleManager::
    MaybeCreateSubframeNavigationFilteringThrottle(
        content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame())
    return nullptr;
  AsyncDocumentSubresourceFilter* parent_filter =
      GetParentFrameFilter(navigation_handle);
  return parent_filter ? std::make_unique<SubframeNavigationFilteringThrottle>(
                             navigation_handle, parent_filter, this)
                       : nullptr;
}

std::unique_ptr<ActivationStateComputingNavigationThrottle>
ContentSubresourceFilterThrottleManager::
    MaybeCreateActivationStateComputingThrottle(
        content::NavigationHandle* navigation_handle) {
  // Main frames: create unconditionally.
  if (navigation_handle->IsInMainFrame()) {
    auto throttle =
        ActivationStateComputingNavigationThrottle::CreateForMainFrame(
            navigation_handle);
    if (base::FeatureList::IsEnabled(kAdTagging)) {
      mojom::ActivationState ad_tagging_state;
      ad_tagging_state.activation_level = mojom::ActivationLevel::kDryRun;
      throttle->NotifyPageActivationWithRuleset(EnsureRulesetHandle(),
                                                ad_tagging_state);
    }
    return throttle;
  }

  // Subframes: create only for frames with activated parents.
  AsyncDocumentSubresourceFilter* parent_filter =
      GetParentFrameFilter(navigation_handle);
  if (!parent_filter)
    return nullptr;
  DCHECK(ruleset_handle_);
  return ActivationStateComputingNavigationThrottle::CreateForSubframe(
      navigation_handle, ruleset_handle_.get(),
      parent_filter->activation_state());
}

AsyncDocumentSubresourceFilter*
ContentSubresourceFilterThrottleManager::GetParentFrameFilter(
    content::NavigationHandle* child_frame_navigation) {
  DCHECK(!child_frame_navigation->IsInMainFrame());
  content::RenderFrameHost* parent = child_frame_navigation->GetParentFrame();
  DCHECK(parent);

  // Filter will be null for those special url navigations that were added in
  // MaybeActivateSubframeSpecialUrls and for subframes with no committed
  // navigation. Return the filter of the first parent with a non-null filter.
  while (parent) {
    auto it = frame_host_filter_map_.find(parent);
    if (it == frame_host_filter_map_.end())
      return nullptr;

    if (it->second)
      return it->second.get();
    parent = it->first->GetParent();
  }

  // Since a null filter is only possible for special navigations of iframes and
  // aborted loads in a subframe, the above loop should have found a filter for
  // at least the top level frame, thus making this unreachable.
  NOTREACHED();
  return nullptr;
}

void ContentSubresourceFilterThrottleManager::MaybeShowNotification() {
  if (current_committed_load_has_notified_disallowed_load_)
    return;

  // This shouldn't happen normally, but in the rare case that an IPC from a
  // previous page arrives late we should guard against it.
  auto it = frame_host_filter_map_.find(web_contents()->GetMainFrame());
  if (it == frame_host_filter_map_.end() ||
      it->second->activation_state().activation_level !=
          mojom::ActivationLevel::kEnabled) {
    return;
  }
  client_->ShowNotification();
  current_committed_load_has_notified_disallowed_load_ = true;
}

VerifiedRuleset::Handle*
ContentSubresourceFilterThrottleManager::EnsureRulesetHandle() {
  if (!ruleset_handle_)
    ruleset_handle_ = std::make_unique<VerifiedRuleset::Handle>(dealer_handle_);
  return ruleset_handle_.get();
}

void ContentSubresourceFilterThrottleManager::
    DestroyRulesetHandleIfNoLongerUsed() {
  if (frame_host_filter_map_.size() + ongoing_activation_throttles_.size() ==
      0u) {
    ruleset_handle_.reset();
  }
}

void ContentSubresourceFilterThrottleManager::OnFrameIsAdSubframe(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  ad_frames_.insert(render_frame_host);

  bool parent_is_ad =
      base::Contains(ad_frames_, render_frame_host->GetParent());
  blink::mojom::AdFrameType ad_frame_type =
      parent_is_ad ? blink::mojom::AdFrameType::kChildAd
                   : blink::mojom::AdFrameType::kRootAd;

  // Replicate ad frame type to this frame's proxies, so that it can be looked
  // up in any process involved in rendering the current page.
  render_frame_host->UpdateAdFrameType(ad_frame_type);

  SubresourceFilterObserverManager::FromWebContents(web_contents())
      ->NotifyAdSubframeDetected(render_frame_host);
}

void ContentSubresourceFilterThrottleManager::DidDisallowFirstSubresource() {
  MaybeShowNotification();
}

void ContentSubresourceFilterThrottleManager::FrameIsAdSubframe() {
  OnFrameIsAdSubframe(receiver_.GetCurrentTargetFrame());
}

void ContentSubresourceFilterThrottleManager::SetDocumentLoadStatistics(
    mojom::DocumentLoadStatisticsPtr statistics) {
  if (statistics_)
    statistics_->OnDocumentLoadStatistics(*statistics);
}

void ContentSubresourceFilterThrottleManager::MaybeActivateSubframeSpecialUrls(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame())
    return;

  if (!ShouldUseParentActivation(navigation_handle->GetURL()))
    return;

  content::RenderFrameHost* frame_host =
      navigation_handle->GetRenderFrameHost();
  if (!frame_host)
    return;

  content::RenderFrameHost* parent = navigation_handle->GetParentFrame();
  DCHECK(parent);
  if (base::Contains(frame_host_filter_map_, parent))
    frame_host_filter_map_[frame_host] = nullptr;
}

}  // namespace subresource_filter

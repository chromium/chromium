// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
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
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
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

namespace {

bool ShouldInheritOpenerActivation(content::NavigationHandle* navigation_handle,
                                   content::RenderFrameHost* frame_host) {
  if (!navigation_handle->IsInMainFrame()) {
    return false;
  }

  // If this navigation is for a special url that did not go through the network
  // stack or if the initial (attempted) load wasn't committed, the frame's
  // activation will not have been set. It should instead be inherited from its
  // same-origin opener (if any). See ShouldInheritParentActivation() for
  // subframes.
  content::RenderFrameHost* opener_rfh =
      navigation_handle->GetWebContents()->GetOpener();
  if (!opener_rfh) {
    return false;
  }

  if (!frame_host->GetLastCommittedOrigin().IsSameOriginWith(
          opener_rfh->GetLastCommittedOrigin())) {
    return false;
  }

  return ShouldInheritActivation(navigation_handle->GetURL()) ||
         !navigation_handle->HasCommitted();
}

bool ShouldInheritParentActivation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame()) {
    return false;
  }
  DCHECK(navigation_handle->GetParentFrame());

  // As with ShouldInheritSameOriginOpenerActivation() except that we inherit
  // from the parent frame as we are a subframe.
  return ShouldInheritActivation(navigation_handle->GetURL()) ||
         !navigation_handle->HasCommitted();
}

}  // namespace

const char ContentSubresourceFilterThrottleManager::
    kContentSubresourceFilterThrottleManagerWebContentsUserDataKey[] =
        "content_subresource_filter_throttle_manager";

// static
void ContentSubresourceFilterThrottleManager::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<SubresourceFilterClient> client,
    VerifiedRulesetDealer::Handle* dealer_handle) {
  if (!base::FeatureList::IsEnabled(kSafeBrowsingSubresourceFilter))
    return;

  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      kContentSubresourceFilterThrottleManagerWebContentsUserDataKey,
      std::make_unique<ContentSubresourceFilterThrottleManager>(
          std::move(client), dealer_handle, web_contents));
}

// static
ContentSubresourceFilterThrottleManager*
ContentSubresourceFilterThrottleManager::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<ContentSubresourceFilterThrottleManager*>(
      web_contents->GetUserData(
          kContentSubresourceFilterThrottleManagerWebContentsUserDataKey));
}

// static
const ContentSubresourceFilterThrottleManager*
ContentSubresourceFilterThrottleManager::FromWebContents(
    const content::WebContents* web_contents) {
  return static_cast<const ContentSubresourceFilterThrottleManager*>(
      web_contents->GetUserData(
          kContentSubresourceFilterThrottleManagerWebContentsUserDataKey));
}

ContentSubresourceFilterThrottleManager::
    ContentSubresourceFilterThrottleManager(
        std::unique_ptr<SubresourceFilterClient> client,
        VerifiedRulesetDealer::Handle* dealer_handle,
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      receiver_(web_contents, this),
      dealer_handle_(dealer_handle),
      client_(std::move(client)) {
  SubresourceFilterObserverManager::CreateForWebContents(web_contents);
  scoped_observation_.Observe(
      SubresourceFilterObserverManager::FromWebContents(web_contents));
}

ContentSubresourceFilterThrottleManager::
    ~ContentSubresourceFilterThrottleManager() {}

void ContentSubresourceFilterThrottleManager::OnSubresourceFilterGoingAway() {
  // Stop observing here because the observer manager could be destroyed by the
  // time this class is destroyed.
  DCHECK(scoped_observation_.IsObserving());
  scoped_observation_.Reset();
}

void ContentSubresourceFilterThrottleManager::RenderFrameDeleted(
    content::RenderFrameHost* frame_host) {
  frame_host_filter_map_.erase(frame_host);
  DestroyRulesetHandleIfNoLongerUsed();
}

void ContentSubresourceFilterThrottleManager::FrameDeleted(
    content::RenderFrameHost* frame_host) {
  int frame_tree_node_id = frame_host->GetFrameTreeNodeId();

  ad_frames_.erase(frame_tree_node_id);
  navigated_frames_.erase(frame_tree_node_id);
  navigation_load_policies_.erase(frame_tree_node_id);
}

// Pull the AsyncDocumentSubresourceFilter and its associated
// mojom::ActivationState out of the activation state computing throttle. Store
// it for later filtering of subframe navigations.
void ContentSubresourceFilterThrottleManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNetErrorCode() != net::OK)
    return;

  auto it =
      ongoing_activation_throttles_.find(navigation_handle->GetNavigationId());
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
      base::Contains(ad_frames_, navigation_handle->GetFrameTreeNodeId());
  DCHECK(!is_ad_subframe || !navigation_handle->IsInMainFrame());

  bool parent_is_ad =
      frame_host->GetParent() &&
      base::Contains(ad_frames_, frame_host->GetParent()->GetFrameTreeNodeId());

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
  ActivationStateComputingNavigationThrottle* throttle = nullptr;
  auto throttle_it =
      ongoing_activation_throttles_.find(navigation_handle->GetNavigationId());
  if (throttle_it != ongoing_activation_throttles_.end()) {
    throttle = throttle_it->second;

    // Make sure not to leak throttle pointers.
    ongoing_activation_throttles_.erase(throttle_it);
  }

  // Do nothing if the navigation finished in the same document.
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  // Cannot get the RFH from |navigation_handle| if there's no committed load.
  content::RenderFrameHost* frame_host =
      navigation_handle->HasCommitted()
          ? navigation_handle->GetRenderFrameHost()
          : navigation_handle->GetWebContents()
                ->UnsafeFindFrameByFrameTreeNodeId(
                    navigation_handle->GetFrameTreeNodeId());
  if (!frame_host) {
    DCHECK(!navigation_handle->HasCommitted());
    return;
  }

  // Do nothing if the navigation was uncommitted and this frame has had a
  // previous navigation. We will keep using the existing activation.
  if (!navigated_frames_.insert(navigation_handle->GetFrameTreeNodeId())
           .second &&
      !navigation_handle->HasCommitted()) {
    return;
  }

  bool did_inherit_opener_activation;
  AsyncDocumentSubresourceFilter* filter = FilterForFinishedNavigation(
      navigation_handle, throttle, frame_host, did_inherit_opener_activation);

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
    RecordUmaHistogramsForMainFrameNavigation(
        navigation_handle,
        filter ? filter->activation_state().activation_level
               : mojom::ActivationLevel::kDisabled,
        did_inherit_opener_activation);
  }

  DestroyRulesetHandleIfNoLongerUsed();
}

AsyncDocumentSubresourceFilter*
ContentSubresourceFilterThrottleManager::FilterForFinishedNavigation(
    content::NavigationHandle* navigation_handle,
    ActivationStateComputingNavigationThrottle* throttle,
    content::RenderFrameHost* frame_host,
    bool& did_inherit_opener_activation) {
  DCHECK(navigation_handle);
  DCHECK(frame_host);

  std::unique_ptr<AsyncDocumentSubresourceFilter> filter;
  base::Optional<mojom::ActivationState> activation_to_inherit;
  did_inherit_opener_activation = false;

  if (navigation_handle->HasCommitted() && throttle) {
    CHECK_EQ(navigation_handle, throttle->navigation_handle());
    filter = throttle->ReleaseFilter();
  }

  // If the frame should inherit its activation then, if it has an activated
  // opener/parent, construct a filter with the inherited activation state. The
  // filter's activation state will be available immediately so a throttle is
  // not required. Instead, we construct the filter synchronously.
  if (ShouldInheritOpenerActivation(navigation_handle, frame_host)) {
    content::RenderFrameHost* opener_rfh =
        navigation_handle->GetWebContents()->GetOpener();
    if (auto* opener_throttle_manager =
            ContentSubresourceFilterThrottleManager::FromWebContents(
                content::WebContents::FromRenderFrameHost(opener_rfh))) {
      activation_to_inherit =
          opener_throttle_manager->GetFrameActivationState(opener_rfh);
      did_inherit_opener_activation = true;
    }
  } else if (ShouldInheritParentActivation(navigation_handle)) {
    // Throttles are only constructed for navigations handled by the network
    // stack and we only release filters for committed navigations. When a
    // navigation redirects from a URL handled by the network stack to
    // about:blank, a filter can already exist here. We replace it to match
    // behavior for other about:blank frames.
    DCHECK(!filter || navigation_handle->GetRedirectChain().size() != 1);
    activation_to_inherit =
        GetFrameActivationState(navigation_handle->GetParentFrame());
  }

  if (activation_to_inherit.has_value() &&
      activation_to_inherit->activation_level !=
          mojom::ActivationLevel::kDisabled) {
    DCHECK(dealer_handle_);

    // This constructs the filter in a way that allows it to be immediately
    // used. See the AsyncDocumentSubresourceFilter constructor for details.
    filter = std::make_unique<AsyncDocumentSubresourceFilter>(
        EnsureRulesetHandle(), frame_host->GetLastCommittedOrigin(),
        activation_to_inherit.value());
  }

  // Make sure `frame_host_filter_map_` is cleaned up if necessary. Otherwise,
  // it is updated below.
  if (!filter) {
    frame_host_filter_map_.erase(frame_host);
    return nullptr;
  }

  base::OnceClosure disallowed_callback(base::BindOnce(
      &ContentSubresourceFilterThrottleManager::MaybeShowNotification,
      weak_ptr_factory_.GetWeakPtr()));
  filter->set_first_disallowed_load_callback(std::move(disallowed_callback));

  AsyncDocumentSubresourceFilter* raw_ptr = filter.get();
  frame_host_filter_map_[frame_host] = std::move(filter);

  return raw_ptr;
}

void ContentSubresourceFilterThrottleManager::
    RecordUmaHistogramsForMainFrameNavigation(
        content::NavigationHandle* navigation_handle,
        const mojom::ActivationLevel& activation_level,
        bool did_inherit_opener_activation) {
  DCHECK(navigation_handle->IsInMainFrame());

  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.PageLoad.ActivationState",
                            activation_level);
  if (did_inherit_opener_activation) {
    UMA_HISTOGRAM_ENUMERATION(
        "SubresourceFilter.PageLoad.ActivationState.DidInherit",
        activation_level);
  }
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

  auto it =
      ongoing_activation_throttles_.find(navigation_handle->GetNavigationId());
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

  int frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();
  navigation_load_policies_[frame_tree_node_id] = load_policy;
  if (is_ad_subframe)
    ad_frames_.insert(frame_tree_node_id);
}

void ContentSubresourceFilterThrottleManager::MaybeAppendNavigationThrottles(
    content::NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  DCHECK(!navigation_handle->IsSameDocument());
  DCHECK(!ShouldInheritActivation(navigation_handle->GetURL()));

  if (navigation_handle->IsInMainFrame() &&
      client_->GetSafeBrowsingDatabaseManager()) {
    throttles->push_back(
        std::make_unique<SubresourceFilterSafeBrowsingActivationThrottle>(
            navigation_handle, client_.get(),
            content::GetIOThreadTaskRunner({}),
            client_->GetSafeBrowsingDatabaseManager()));
  }

  if (!dealer_handle_)
    return;
  if (auto filtering_throttle =
          MaybeCreateSubframeNavigationFilteringThrottle(navigation_handle)) {
    throttles->push_back(std::move(filtering_throttle));
  }

  DCHECK(!base::Contains(ongoing_activation_throttles_,
                         navigation_handle->GetNavigationId()));
  if (auto activation_throttle =
          MaybeCreateActivationStateComputingThrottle(navigation_handle)) {
    ongoing_activation_throttles_[navigation_handle->GetNavigationId()] =
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
         base::Contains(ad_frames_, frame_host->GetFrameTreeNodeId()) ||
         base::Contains(ad_frames_, parent_frame->GetFrameTreeNodeId());
}

bool ContentSubresourceFilterThrottleManager::IsFrameTaggedAsAd(
    content::RenderFrameHost* frame_host) const {
  return frame_host &&
         base::Contains(ad_frames_, frame_host->GetFrameTreeNodeId());
}

base::Optional<LoadPolicy>
ContentSubresourceFilterThrottleManager::LoadPolicyForLastCommittedNavigation(
    content::RenderFrameHost* frame_host) const {
  if (!frame_host)
    return base::nullopt;
  auto it = navigation_load_policies_.find(frame_host->GetFrameTreeNodeId());
  if (it == navigation_load_policies_.end())
    return base::nullopt;
  return it->second;
}

void ContentSubresourceFilterThrottleManager::OnReloadRequested() {
  client_->OnReloadRequested();
}

// static
void ContentSubresourceFilterThrottleManager::LogAction(
    SubresourceFilterAction action) {
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.Actions2", action);
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
  return GetFrameFilter(parent);
}

const base::Optional<subresource_filter::mojom::ActivationState>
ContentSubresourceFilterThrottleManager::GetFrameActivationState(
    content::RenderFrameHost* frame_host) {
  if (AsyncDocumentSubresourceFilter* filter = GetFrameFilter(frame_host))
    return filter->activation_state();
  return base::nullopt;
}

AsyncDocumentSubresourceFilter*
ContentSubresourceFilterThrottleManager::GetFrameFilter(
    content::RenderFrameHost* frame_host) {
  DCHECK(frame_host);

  auto it = frame_host_filter_map_.find(frame_host);
  if (it == frame_host_filter_map_.end())
    return nullptr;

  DCHECK(it->second);
  return it->second.get();
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

  ad_frames_.insert(render_frame_host->GetFrameTreeNodeId());

  bool parent_is_ad = base::Contains(
      ad_frames_, render_frame_host->GetParent()->GetFrameTreeNodeId());
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

void ContentSubresourceFilterThrottleManager::OnAdsViolationTriggered(
    mojom::AdsViolation violation) {
  client_->OnAdsViolationTriggered(
      receiver_.GetCurrentTargetFrame()->GetMainFrame(), violation);
}

}  // namespace subresource_filter

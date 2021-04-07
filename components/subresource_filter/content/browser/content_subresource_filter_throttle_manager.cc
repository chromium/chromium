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
#include "components/subresource_filter/content/browser/profile_interaction_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
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
#include "content/public/common/url_utils.h"
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
    SubresourceFilterProfileContext* profile_context,
    VerifiedRulesetDealer::Handle* dealer_handle) {
  if (!base::FeatureList::IsEnabled(kSafeBrowsingSubresourceFilter))
    return;

  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      kContentSubresourceFilterThrottleManagerWebContentsUserDataKey,
      std::make_unique<ContentSubresourceFilterThrottleManager>(
          std::move(client), profile_context, dealer_handle, web_contents));
}

// static
ContentSubresourceFilterThrottleManager*
ContentSubresourceFilterThrottleManager::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<ContentSubresourceFilterThrottleManager*>(
      web_contents->GetUserData(
          kContentSubresourceFilterThrottleManagerWebContentsUserDataKey));
}

ContentSubresourceFilterThrottleManager::
    ContentSubresourceFilterThrottleManager(
        std::unique_ptr<SubresourceFilterClient> client,
        SubresourceFilterProfileContext* profile_context,
        VerifiedRulesetDealer::Handle* dealer_handle,
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      receiver_(web_contents, this),
      dealer_handle_(dealer_handle),
      client_(std::move(client)),
      profile_interaction_manager_(
          std::make_unique<subresource_filter::ProfileInteractionManager>(
              web_contents,
              profile_context)) {
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
    int frame_tree_node_id) {
  ad_frames_.erase(frame_tree_node_id);
  navigated_frames_.erase(frame_tree_node_id);
  navigation_load_policies_.erase(frame_tree_node_id);
  tracked_ad_evidence_.erase(frame_tree_node_id);
}

// Pull the AsyncDocumentSubresourceFilter and its associated
// mojom::ActivationState out of the activation state computing throttle. Store
// it for later filtering of subframe navigations.
void ContentSubresourceFilterThrottleManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  content::RenderFrameHost* frame_host =
      navigation_handle->GetRenderFrameHost();

  // Update the ad status of a frame given the new navigation. This may tag or
  // untag a frame as an ad.
  if (!navigation_handle->IsInMainFrame()) {
    blink::FrameAdEvidence& ad_evidence = EnsureFrameAdEvidence(frame_host);
    ad_evidence.set_is_complete();

    SetIsAdSubframe(frame_host, ad_evidence.IndicatesAdSubframe());
  }

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
      "activation_state", static_cast<int>(level), "render_frame_host",
      frame_host);

  throttle->WillSendActivationToRenderer();

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

  int frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();

  // Cannot get the RFH from |navigation_handle| if there's no committed load.
  content::RenderFrameHost* frame_host =
      navigation_handle->HasCommitted()
          ? navigation_handle->GetRenderFrameHost()
          : navigation_handle->GetWebContents()
                ->UnsafeFindFrameByFrameTreeNodeId(frame_tree_node_id);
  if (!frame_host) {
    DCHECK(!navigation_handle->HasCommitted());
    return;
  }

  // Do nothing if the navigation was uncommitted and this frame has had a
  // previous navigation. We will keep using the existing activation.
  bool is_initial_navigation =
      navigated_frames_.insert(frame_tree_node_id).second;
  if (!is_initial_navigation && !navigation_handle->HasCommitted()) {
    return;
  }

  // Finish setting FrameAdEvidence fields on initial subframe navigations that
  // did not pass through `ReadyToCommitNavigation()`. Note that initial
  // navigations to about:blank commit synchronously. We handle navigations
  // there where possible to ensure that any messages to the renderer contain
  // the right ad status.
  if (is_initial_navigation && !navigation_handle->IsInMainFrame() &&
      !(navigation_handle->HasCommitted() &&
        !navigation_handle->GetURL().IsAboutBlank()) &&
      !navigation_handle->IsWaitingToCommit() &&
      !base::Contains(ad_frames_, frame_tree_node_id)) {
    EnsureFrameAdEvidence(frame_host).set_is_complete();

    // Initial synchronous navigations to about:blank should only be tagged by
    // the renderer. Currently, an aborted initial load to a URL matching the
    // filter list incorrectly has its load policy saved. We avoid tagging it as
    // an ad here to ensure frames are always tagged before DidFinishNavigation.
    // TODO(crbug.com/1148058): Once these load policies are no longer saved,
    // update the DCHECK to verify that the evidence doesn't indicate a subframe
    // (regardless of the URL).
    DCHECK(!(navigation_handle->GetURL().IsAboutBlank() &&
             EnsureFrameAdEvidence(frame_host).IndicatesAdSubframe()));
  } else {
    DCHECK(navigation_handle->IsInMainFrame() ||
           EnsureFrameAdEvidence(frame_host).is_complete());
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
    LoadPolicy load_policy) {
  DCHECK(!navigation_handle->IsInMainFrame());

  int frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();
  navigation_load_policies_[frame_tree_node_id] = load_policy;

  // TODO(crbug.com/843646): Use an API that NavigationHandle supports rather
  // than trying to infer what the NavigationHandle is doing.
  content::RenderFrameHost* starting_rfh =
      navigation_handle->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
          navigation_handle->GetFrameTreeNodeId());
  DCHECK(starting_rfh);

  blink::FrameAdEvidence& ad_evidence = EnsureFrameAdEvidence(starting_rfh);
  DCHECK_EQ(ad_evidence.parent_is_ad(),
            base::Contains(ad_frames_,
                           starting_rfh->GetParent()->GetFrameTreeNodeId()));

  ad_evidence.UpdateFilterListResult(
      InterpretLoadPolicyAsEvidence(load_policy));
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
            navigation_handle, profile_interaction_manager_.get(),
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
  profile_interaction_manager_->OnReloadRequested();
}

void ContentSubresourceFilterThrottleManager::OnAdsViolationTriggered(
    content::RenderFrameHost* rfh,
    mojom::AdsViolation triggered_violation) {
  profile_interaction_manager_->OnAdsViolationTriggered(rfh,
                                                        triggered_violation);
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
                             navigation_handle, parent_filter)
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

  profile_interaction_manager_->MaybeShowNotification(client_.get());

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
  // `FrameIsAdSubframe()` can only be called for an initial empty document. As
  // it won't pass through `ReadyToCommitNavigation()` (and has not yet passed
  // through `DidFinishNavigation()`), we know it won't be updated further.
  EnsureFrameAdEvidence(render_frame_host).set_is_complete();

  // The renderer has indicated that the frame is an ad.
  SetIsAdSubframe(render_frame_host, /*is_ad_subframe=*/true);
}

void ContentSubresourceFilterThrottleManager::SetIsAdSubframe(
    content::RenderFrameHost* render_frame_host,
    bool is_ad_subframe) {
  int frame_tree_node_id = render_frame_host->GetFrameTreeNodeId();
  DCHECK(base::Contains(tracked_ad_evidence_, frame_tree_node_id));
  DCHECK_EQ(tracked_ad_evidence_.at(frame_tree_node_id).IndicatesAdSubframe(),
            is_ad_subframe);
  DCHECK(render_frame_host->GetParent());

  // `ad_frames_` does not need updating.
  if (is_ad_subframe == base::Contains(ad_frames_, frame_tree_node_id))
    return;

  blink::mojom::AdFrameType ad_frame_type = blink::mojom::AdFrameType::kNonAd;
  if (is_ad_subframe) {
    ad_frames_.insert(frame_tree_node_id);

    bool parent_is_ad = base::Contains(
        ad_frames_, render_frame_host->GetParent()->GetFrameTreeNodeId());
    ad_frame_type = parent_is_ad ? blink::mojom::AdFrameType::kChildAd
                                 : blink::mojom::AdFrameType::kRootAd;
  } else {
    ad_frames_.erase(frame_tree_node_id);
  }

  // Replicate ad frame type to this frame's proxies, so that it can be looked
  // up in any process involved in rendering the current page.
  render_frame_host->UpdateAdFrameType(ad_frame_type);

  SubresourceFilterObserverManager::FromWebContents(web_contents())
      ->NotifyIsAdSubframeChanged(render_frame_host, is_ad_subframe);
}

void ContentSubresourceFilterThrottleManager::SetIsAdSubframeForTesting(
    content::RenderFrameHost* render_frame_host,
    bool is_ad_subframe) {
  DCHECK(render_frame_host->GetParent());
  if (is_ad_subframe ==
      base::Contains(ad_frames_, render_frame_host->GetFrameTreeNodeId())) {
    return;
  }

  if (is_ad_subframe) {
    // We mark the frame as matching a blocking rule so that the ad evidence
    // indicates an ad subframe.
    EnsureFrameAdEvidence(render_frame_host)
        .UpdateFilterListResult(
            blink::mojom::FilterListResult::kMatchedBlockingRule);
    OnFrameIsAdSubframe(render_frame_host);
  } else {
    // There's currently no legal transition that can untag a frame. Instead, to
    // mimic future behavior, we simply replace the FrameAdEvidence.
    // TODO(crbug.com/1101584): Replace with legal transition when one exists.
    tracked_ad_evidence_.erase(render_frame_host->GetFrameTreeNodeId());
    EnsureFrameAdEvidence(render_frame_host).set_is_complete();
  }
}

base::Optional<blink::FrameAdEvidence>
ContentSubresourceFilterThrottleManager::GetAdEvidenceForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto tracked_ad_evidence_it =
      tracked_ad_evidence_.find(render_frame_host->GetFrameTreeNodeId());
  if (tracked_ad_evidence_it == tracked_ad_evidence_.end())
    return base::nullopt;
  return tracked_ad_evidence_it->second;
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
  OnAdsViolationTriggered(receiver_.GetCurrentTargetFrame()->GetMainFrame(),
                          violation);
}

void ContentSubresourceFilterThrottleManager::SubframeWasCreatedByAdScript() {
  OnSubframeWasCreatedByAdScript(receiver_.GetCurrentTargetFrame());
}

void ContentSubresourceFilterThrottleManager::OnSubframeWasCreatedByAdScript(
    content::RenderFrameHost* frame_host) {
  DCHECK(frame_host);

  if (!frame_host->GetParent()) {
    return;
  }

  EnsureFrameAdEvidence(frame_host)
      .set_created_by_ad_script(
          blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
}

blink::FrameAdEvidence&
ContentSubresourceFilterThrottleManager::EnsureFrameAdEvidence(
    content::RenderFrameHost* frame_host) {
  DCHECK(frame_host);
  DCHECK(frame_host->GetParent());
  return tracked_ad_evidence_
      .emplace(frame_host->GetFrameTreeNodeId(),
               /*parent_is_ad=*/base::Contains(
                   ad_frames_, frame_host->GetParent()->GetFrameTreeNodeId()))
      .first->second;
}

}  // namespace subresource_filter

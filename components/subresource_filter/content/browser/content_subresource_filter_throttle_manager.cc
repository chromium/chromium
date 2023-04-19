// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/subresource_filter/content/browser/activation_state_computing_navigation_throttle.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/content/browser/page_load_statistics.h"
#include "components/subresource_filter/content/browser/profile_interaction_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"
#include "components/subresource_filter/content/mojom/subresource_filter.mojom.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace subresource_filter {

namespace {

bool ShouldInheritOpenerActivation(content::NavigationHandle* navigation_handle,
                                   content::RenderFrameHost* frame_host) {
  // TODO(bokan): Add and use GetOpener associated with `frame_host`'s Page.
  // https://crbug.com/1230153.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
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
  // TODO(crbug.com/1263541): Investigate if this should apply to fenced frames
  // as well, or if we can default them to unactivated initially.
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

// static
const int ContentSubresourceFilterThrottleManager::kUserDataKey;

// static
void ContentSubresourceFilterThrottleManager::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::SubresourceFilterHost>
        pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  if (auto* manager = FromPage(render_frame_host->GetPage()))
    manager->receiver_.Bind(render_frame_host, std::move(pending_receiver));
}

// static
std::unique_ptr<ContentSubresourceFilterThrottleManager>
ContentSubresourceFilterThrottleManager::CreateForNewPage(
    SubresourceFilterProfileContext* profile_context,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    VerifiedRulesetDealer::Handle* dealer_handle,
    ContentSubresourceFilterWebContentsHelper& web_contents_helper,
    content::NavigationHandle& initiating_navigation_handle) {
  DCHECK(IsInSubresourceFilterRoot(&initiating_navigation_handle));

  if (!base::FeatureList::IsEnabled(kSafeBrowsingSubresourceFilter))
    return nullptr;

  return std::make_unique<ContentSubresourceFilterThrottleManager>(
      profile_context, database_manager, dealer_handle, web_contents_helper,
      initiating_navigation_handle);
}

// static
ContentSubresourceFilterThrottleManager*
ContentSubresourceFilterThrottleManager::FromPage(content::Page& page) {
  return ContentSubresourceFilterWebContentsHelper::GetThrottleManager(page);
}

// static
ContentSubresourceFilterThrottleManager*
ContentSubresourceFilterThrottleManager::FromNavigationHandle(
    content::NavigationHandle& navigation_handle) {
  return ContentSubresourceFilterWebContentsHelper::GetThrottleManager(
      navigation_handle);
}

ContentSubresourceFilterThrottleManager::
    ContentSubresourceFilterThrottleManager(
        SubresourceFilterProfileContext* profile_context,
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
            database_manager,
        VerifiedRulesetDealer::Handle* dealer_handle,
        ContentSubresourceFilterWebContentsHelper& web_contents_helper,
        content::NavigationHandle& initiating_navigation_handle)
    : receiver_(initiating_navigation_handle.GetWebContents(), this),
      dealer_handle_(dealer_handle),
      database_manager_(std::move(database_manager)),
      profile_interaction_manager_(
          std::make_unique<subresource_filter::ProfileInteractionManager>(
              profile_context)),
      web_contents_helper_(web_contents_helper) {}

ContentSubresourceFilterThrottleManager::
    ~ContentSubresourceFilterThrottleManager() {
  web_contents_helper_->WillDestroyThrottleManager(this);
}

void ContentSubresourceFilterThrottleManager::RenderFrameDeleted(
    content::RenderFrameHost* frame_host) {
  frame_host_filter_map_.erase(frame_host);
  DestroyRulesetHandleIfNoLongerUsed();
}

void ContentSubresourceFilterThrottleManager::FrameDeleted(
    int frame_tree_node_id) {
  // TODO(bokan): This will be called for frame tree nodes that don't belong to
  // this frame tree node as well since we can't tell outside of //content
  // which page a FTN belongs to.
  ad_frames_.erase(frame_tree_node_id);
  navigation_load_policies_.erase(frame_tree_node_id);
  tracked_ad_evidence_.erase(frame_tree_node_id);
}

// Pull the AsyncDocumentSubresourceFilter and its associated
// mojom::ActivationState out of the activation state computing throttle. Store
// it for later filtering of child frame navigations.
void ContentSubresourceFilterThrottleManager::ReadyToCommitInFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  ready_to_commit_navigations_.insert(navigation_handle->GetNavigationId());

  content::RenderFrameHost* frame_host =
      navigation_handle->GetRenderFrameHost();

  absl::optional<blink::FrameAdEvidence> ad_evidence_for_navigation;

  // Update the ad status of a frame given the new navigation. This may tag or
  // untag a frame as an ad.
  if (!IsInSubresourceFilterRoot(navigation_handle)) {
    blink::FrameAdEvidence& ad_evidence =
        EnsureFrameAdEvidence(navigation_handle);
    DCHECK_EQ(
        ad_evidence.parent_is_ad(),
        base::Contains(
            ad_frames_,
            frame_host->GetParentOrOuterDocument()->GetFrameTreeNodeId()));
    ad_evidence.set_is_complete();
    ad_evidence_for_navigation = ad_evidence;

    SetIsAdFrame(frame_host, ad_evidence.IndicatesAdFrame());
  }

  mojom::ActivationState activation_state =
      ActivationStateForNextCommittedLoad(navigation_handle);

  TRACE_EVENT2(
      TRACE_DISABLED_BY_DEFAULT("loading"),
      "ContentSubresourceFilterThrottleManager::ReadyToCommitNavigation",
      "activation_state", static_cast<int>(activation_state.activation_level),
      "render_frame_host", navigation_handle->GetRenderFrameHost());

  mojo::AssociatedRemote<mojom::SubresourceFilterAgent> agent;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&agent);

  // We send `ad_evidence_for_navigation` even if the frame is not tagged as an
  // ad. This ensures the renderer's copy is up-to-date, including propagating
  // it on cross-process navigations.
  agent->ActivateForNextCommittedLoad(activation_state.Clone(),
                                      ad_evidence_for_navigation);
}

mojom::ActivationState
ContentSubresourceFilterThrottleManager::ActivationStateForNextCommittedLoad(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNetErrorCode() != net::OK)
    return mojom::ActivationState();

  auto it =
      ongoing_activation_throttles_.find(navigation_handle->GetNavigationId());
  if (it == ongoing_activation_throttles_.end())
    return mojom::ActivationState();

  // Main frame throttles with disabled page-level activation will not have
  // associated filters.
  ActivationStateComputingNavigationThrottle* throttle = it->second;
  AsyncDocumentSubresourceFilter* filter = throttle->filter();
  if (!filter)
    return mojom::ActivationState();

  // A filter with DISABLED activation indicates a corrupted ruleset.
  if (filter->activation_state().activation_level ==
      mojom::ActivationLevel::kDisabled) {
    return mojom::ActivationState();
  }

  throttle->WillSendActivationToRenderer();
  return filter->activation_state();
}

void ContentSubresourceFilterThrottleManager::DidFinishInFrameNavigation(
    content::NavigationHandle* navigation_handle,
    bool is_initial_navigation) {
  ActivationStateComputingNavigationThrottle* throttle = nullptr;
  auto throttle_it =
      ongoing_activation_throttles_.find(navigation_handle->GetNavigationId());
  if (throttle_it != ongoing_activation_throttles_.end()) {
    throttle = throttle_it->second;

    // Make sure not to leak throttle pointers.
    ongoing_activation_throttles_.erase(throttle_it);
  }

  bool passed_through_ready_to_commit =
      ready_to_commit_navigations_.erase(navigation_handle->GetNavigationId());

  // Do nothing if the navigation finished in the same document.
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  // Do nothing if the frame was destroyed.
  if (navigation_handle->IsWaitingToCommit() &&
      navigation_handle->GetRenderFrameHost()->GetLifecycleState() ==
          content::RenderFrameHost::LifecycleState::kPendingDeletion) {
    return;
  }

  content::RenderFrameHost* frame_host =
      (navigation_handle->HasCommitted() ||
       navigation_handle->IsWaitingToCommit())
          ? navigation_handle->GetRenderFrameHost()
          : content::RenderFrameHost::FromID(
                navigation_handle->GetPreviousRenderFrameHostId());
  if (!frame_host)
    return;

  RecordExperimentalUmaHistogramsForNavigation(navigation_handle,
                                               passed_through_ready_to_commit);

  const int frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();

  // Do nothing if the navigation was uncommitted and this frame has had a
  // previous navigation. We will keep using the previous page's throttle
  // manager and this one will be deleted.
  if (!is_initial_navigation && !navigation_handle->HasCommitted()) {
    return;
  }

  // Finish setting FrameAdEvidence fields on initial child frame navigations
  // that did not pass through `ReadyToCommitNavigation()`. Note that initial
  // navigations to about:blank commit synchronously. We handle navigations
  // there where possible to ensure that any messages to the renderer contain
  // the right ad status.
  // TODO(crbug.com/1263541): Investigate how to handle fenced frames here.
  if (is_initial_navigation && !navigation_handle->IsInMainFrame() &&
      !(navigation_handle->HasCommitted() &&
        !navigation_handle->GetURL().IsAboutBlank()) &&
      !navigation_handle->IsWaitingToCommit() &&
      !base::Contains(ad_frames_, frame_tree_node_id)) {
    EnsureFrameAdEvidence(navigation_handle).set_is_complete();

    // Initial synchronous navigations to about:blank should only be tagged by
    // the renderer. Currently, an aborted initial load to a URL matching the
    // filter list incorrectly has its load policy saved. We avoid tagging it as
    // an ad here to ensure frames are always tagged before DidFinishNavigation.
    // TODO(crbug.com/1148058): Once these load policies are no longer saved,
    // update the DCHECK to verify that the evidence doesn't indicate a subframe
    // (regardless of the URL).
    DCHECK(!(navigation_handle->GetURL().IsAboutBlank() &&
             EnsureFrameAdEvidence(navigation_handle).IndicatesAdFrame()));
  } else {
    DCHECK(navigation_handle->IsInMainFrame() ||
           EnsureFrameAdEvidence(navigation_handle).is_complete());
  }

  bool did_inherit_opener_activation;
  AsyncDocumentSubresourceFilter* filter = FilterForFinishedNavigation(
      navigation_handle, throttle, frame_host, did_inherit_opener_activation);

  if (IsInSubresourceFilterRoot(navigation_handle)) {
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
    RecordUmaHistogramsForRootNavigation(
        navigation_handle,
        filter ? filter->activation_state().activation_level
               : mojom::ActivationLevel::kDisabled,
        did_inherit_opener_activation);
  }

  DestroyRulesetHandleIfNoLongerUsed();
}

void ContentSubresourceFilterThrottleManager::
    RecordExperimentalUmaHistogramsForNavigation(
        content::NavigationHandle* navigation_handle,
        bool passed_through_ready_to_commit) {
  // For subframe navigations that pass through ready to commit, we record
  // whether they eventually committed. We also break this out by whether the
  // navigation matches the restricted navigation heuristic and by ad status.
  // The observed frequency will reveal the scope of current mishandling of such
  // navigations by Ad Tagging. Navigations to URLs that inherit activation
  // (e.g. about:srcdoc) are excluded as no load policy would be calculated.
  // Navigations with dead RenderFrames are also excluded as any load policy
  // sent to the renderer won't be used.
  // TODO(alexmt): Remove once frequency is determined.
  // TODO(crbug.com/1263541): Record histograms for fenced frame roots and fix
  // |is_same_domain_to_main_frame_| below.
  if (!passed_through_ready_to_commit || navigation_handle->IsInMainFrame() ||
      ShouldInheritActivation(navigation_handle->GetURL()) ||
      !navigation_handle->GetRenderFrameHost()->IsRenderFrameLive()) {
    return;
  }

  base::UmaHistogramBoolean(
      "SubresourceFilter.Experimental.ReadyToCommitResultsInCommit2",
      navigation_handle->HasCommitted());
  blink::mojom::FilterListResult latest_filter_list_result =
      EnsureFrameAdEvidence(navigation_handle).latest_filter_list_result();
  bool is_same_domain_to_outermost_main_frame =
      net::registry_controlled_domains::SameDomainOrHost(
          navigation_handle->GetURL(),
          navigation_handle->GetRenderFrameHost()
              ->GetOutermostMainFrame()
              ->GetLastCommittedURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  bool is_restricted_navigation =
      latest_filter_list_result ==
          blink::mojom::FilterListResult::kMatchedAllowingRule ||
      (latest_filter_list_result ==
           blink::mojom::FilterListResult::kMatchedNoRules &&
       is_same_domain_to_outermost_main_frame);
  if (is_restricted_navigation &&
      base::Contains(ad_frames_, navigation_handle->GetFrameTreeNodeId())) {
    base::UmaHistogramBoolean(
        "SubresourceFilter.Experimental.ReadyToCommitResultsInCommit2."
        "RestrictedAdFrameNavigation",
        navigation_handle->HasCommitted());
  }
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
  absl::optional<mojom::ActivationState> activation_to_inherit;
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
    if (auto* opener_throttle_manager = FromPage(opener_rfh->GetPage())) {
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

  // Safe to pass unowned |frame_host| pointer since the filter that owns this
  // callback is owned in the frame_host_filter_map_ which will be removed when
  // the RenderFrameHost is deleted.
  base::OnceClosure disallowed_callback(base::BindOnce(
      &ContentSubresourceFilterThrottleManager::MaybeShowNotification,
      weak_ptr_factory_.GetWeakPtr(), frame_host));
  filter->set_first_disallowed_load_callback(std::move(disallowed_callback));

  AsyncDocumentSubresourceFilter* filter_ptr = filter.get();
  frame_host_filter_map_[frame_host] = std::move(filter);

  return filter_ptr;
}

void ContentSubresourceFilterThrottleManager::
    RecordUmaHistogramsForRootNavigation(
        content::NavigationHandle* navigation_handle,
        const mojom::ActivationLevel& activation_level,
        bool did_inherit_opener_activation) {
  DCHECK(IsInSubresourceFilterRoot(navigation_handle));

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
  if (!statistics_ || render_frame_host != &page_->GetMainDocument())
    return;
  statistics_->OnDidFinishLoad();
}

void ContentSubresourceFilterThrottleManager::DidBecomePrimaryPage() {
  DCHECK(page_);
  DCHECK(page_->IsPrimary());
  // If we tried to notify while non-primary, we didn't show UI so do that now
  // that the page became primary. This also leads to reattempting notification
  // if a page transitioned from primary to non-primary and back (BFCache).
  if (current_committed_load_has_notified_disallowed_load_)
    profile_interaction_manager_->MaybeShowNotification();
}

void ContentSubresourceFilterThrottleManager::OnPageCreated(
    content::Page& page) {
  DCHECK(!page.GetMainDocument().IsFencedFrameRoot());
  DCHECK(!page_);
  page_ = &page;
  profile_interaction_manager_->DidCreatePage(*page_);
}

// Sets the desired page-level |activation_state| for the currently ongoing
// page load, identified by its main-frame |navigation_handle|. If this method
// is not called for a main-frame navigation, the default behavior is no
// activation for that page load.
void ContentSubresourceFilterThrottleManager::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const mojom::ActivationState& activation_state) {
  DCHECK(IsInSubresourceFilterRoot(navigation_handle));
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

void ContentSubresourceFilterThrottleManager::OnChildFrameNavigationEvaluated(
    content::NavigationHandle* navigation_handle,
    LoadPolicy load_policy) {
  DCHECK(!IsInSubresourceFilterRoot(navigation_handle));
  if (!navigation_handle->GetParentFrameOrOuterDocument())
    return;

  int frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();
  navigation_load_policies_[frame_tree_node_id] = load_policy;

  blink::FrameAdEvidence& ad_evidence =
      EnsureFrameAdEvidence(navigation_handle);
  DCHECK_EQ(ad_evidence.parent_is_ad(),
            base::Contains(ad_frames_,
                           navigation_handle->GetParentFrameOrOuterDocument()
                               ->GetFrameTreeNodeId()));

  ad_evidence.UpdateFilterListResult(
      InterpretLoadPolicyAsEvidence(load_policy));
}

void ContentSubresourceFilterThrottleManager::MaybeAppendNavigationThrottles(
    content::NavigationHandle* navigation_handle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  DCHECK(!navigation_handle->IsSameDocument());
  DCHECK(!ShouldInheritActivation(navigation_handle->GetURL()));

  if (IsInSubresourceFilterRoot(navigation_handle) && database_manager_) {
    throttles->push_back(
        std::make_unique<SubresourceFilterSafeBrowsingActivationThrottle>(
            navigation_handle, profile_interaction_manager_.get(),
            content::GetIOThreadTaskRunner({}), database_manager_));
  }

  if (!dealer_handle_)
    return;
  if (auto filtering_throttle =
          MaybeCreateChildFrameNavigationFilteringThrottle(navigation_handle)) {
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
    int frame_tree_node_id) const {
  return base::Contains(ad_frames_, frame_tree_node_id);
}

bool ContentSubresourceFilterThrottleManager::IsRenderFrameHostTaggedAsAd(
    content::RenderFrameHost* frame_host) const {
  if (!frame_host)
    return false;

  return IsFrameTaggedAsAd(frame_host->GetFrameTreeNodeId());
}

absl::optional<LoadPolicy>
ContentSubresourceFilterThrottleManager::LoadPolicyForLastCommittedNavigation(
    int frame_tree_node_id) const {
  auto it = navigation_load_policies_.find(frame_tree_node_id);
  if (it == navigation_load_policies_.end())
    return absl::nullopt;
  return it->second;
}

void ContentSubresourceFilterThrottleManager::OnReloadRequested() {
  DCHECK(page_);
  DCHECK(page_->IsPrimary());
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

std::unique_ptr<ChildFrameNavigationFilteringThrottle>
ContentSubresourceFilterThrottleManager::
    MaybeCreateChildFrameNavigationFilteringThrottle(
        content::NavigationHandle* navigation_handle) {
  if (IsInSubresourceFilterRoot(navigation_handle))
    return nullptr;
  AsyncDocumentSubresourceFilter* parent_filter =
      GetParentFrameFilter(navigation_handle);
  return parent_filter
             ? std::make_unique<ChildFrameNavigationFilteringThrottle>(
                   navigation_handle, parent_filter)
             : nullptr;
}

std::unique_ptr<ActivationStateComputingNavigationThrottle>
ContentSubresourceFilterThrottleManager::
    MaybeCreateActivationStateComputingThrottle(
        content::NavigationHandle* navigation_handle) {
  // Subresource filter roots: create unconditionally.
  if (IsInSubresourceFilterRoot(navigation_handle)) {
    auto throttle = ActivationStateComputingNavigationThrottle::CreateForRoot(
        navigation_handle);
    if (base::FeatureList::IsEnabled(kAdTagging)) {
      mojom::ActivationState ad_tagging_state;
      ad_tagging_state.activation_level = mojom::ActivationLevel::kDryRun;
      throttle->NotifyPageActivationWithRuleset(EnsureRulesetHandle(),
                                                ad_tagging_state);
    }
    return throttle;
  }

  // Subresource filter children: create only for frames with activated
  // parents.
  AsyncDocumentSubresourceFilter* parent_filter =
      GetParentFrameFilter(navigation_handle);
  if (!parent_filter)
    return nullptr;
  DCHECK(ruleset_handle_);
  return ActivationStateComputingNavigationThrottle::CreateForChild(
      navigation_handle, ruleset_handle_.get(),
      parent_filter->activation_state());
}

AsyncDocumentSubresourceFilter*
ContentSubresourceFilterThrottleManager::GetParentFrameFilter(
    content::NavigationHandle* child_frame_navigation) {
  DCHECK(!IsInSubresourceFilterRoot(child_frame_navigation));
  return GetFrameFilter(
      child_frame_navigation->GetParentFrameOrOuterDocument());
}

const absl::optional<subresource_filter::mojom::ActivationState>
ContentSubresourceFilterThrottleManager::GetFrameActivationState(
    content::RenderFrameHost* frame_host) {
  if (AsyncDocumentSubresourceFilter* filter = GetFrameFilter(frame_host))
    return filter->activation_state();
  return absl::nullopt;
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

void ContentSubresourceFilterThrottleManager::MaybeShowNotification(
    content::RenderFrameHost* frame_host) {
  DCHECK(page_);
  DCHECK_EQ(&GetSubresourceFilterRootPage(frame_host), page_);

  if (current_committed_load_has_notified_disallowed_load_)
    return;

  auto it = frame_host_filter_map_.find(&page_->GetMainDocument());
  if (it == frame_host_filter_map_.end() ||
      it->second->activation_state().activation_level !=
          mojom::ActivationLevel::kEnabled) {
    return;
  }

  current_committed_load_has_notified_disallowed_load_ = true;

  // Non-primary pages shouldn't affect UI. When the page becomes primary we'll
  // check |current_committed_load_has_notified_disallowed_load_| and try
  // again.
  if (page_->IsPrimary())
    profile_interaction_manager_->MaybeShowNotification();
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

void ContentSubresourceFilterThrottleManager::OnFrameIsAd(
    content::RenderFrameHost* render_frame_host) {
  EnsureFrameAdEvidence(render_frame_host).set_is_complete();

  // The renderer has indicated that the frame is an ad.
  SetIsAdFrame(render_frame_host, /*is_ad_frame=*/true);
}

void ContentSubresourceFilterThrottleManager::SetIsAdFrame(
    content::RenderFrameHost* render_frame_host,
    bool is_ad_frame) {
  int frame_tree_node_id = render_frame_host->GetFrameTreeNodeId();
  DCHECK(base::Contains(tracked_ad_evidence_, frame_tree_node_id));
  DCHECK_EQ(tracked_ad_evidence_.at(frame_tree_node_id).IndicatesAdFrame(),
            is_ad_frame);
  DCHECK(render_frame_host->GetParentOrOuterDocument());

  // `ad_frames_` does not need updating.
  if (is_ad_frame == base::Contains(ad_frames_, frame_tree_node_id))
    return;

  if (is_ad_frame) {
    ad_frames_.insert(frame_tree_node_id);
  } else {
    ad_frames_.erase(frame_tree_node_id);
  }

  // Replicate `is_ad_frame` to this frame's proxies, so that it can be
  // looked up in any process involved in rendering the current page.
  render_frame_host->UpdateIsAdFrame(is_ad_frame);

  SubresourceFilterObserverManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host))
      ->NotifyIsAdFrameChanged(render_frame_host, is_ad_frame);
}

void ContentSubresourceFilterThrottleManager::SetIsAdFrameForTesting(
    content::RenderFrameHost* render_frame_host,
    bool is_ad_frame) {
  DCHECK(render_frame_host->GetParentOrOuterDocument());
  if (is_ad_frame ==
      base::Contains(ad_frames_, render_frame_host->GetFrameTreeNodeId())) {
    return;
  }

  if (is_ad_frame) {
    // We mark the frame as matching a blocking rule so that the ad evidence
    // indicates an ad frame.
    EnsureFrameAdEvidence(render_frame_host)
        .UpdateFilterListResult(
            blink::mojom::FilterListResult::kMatchedBlockingRule);
    OnFrameIsAd(render_frame_host);
  } else {
    // There's currently no legal transition that can untag a frame. Instead, to
    // mimic future behavior, we simply replace the FrameAdEvidence.
    // TODO(crbug.com/1101584): Replace with legal transition when one exists.
    tracked_ad_evidence_.erase(render_frame_host->GetFrameTreeNodeId());
    EnsureFrameAdEvidence(render_frame_host).set_is_complete();
  }
}

absl::optional<blink::FrameAdEvidence>
ContentSubresourceFilterThrottleManager::GetAdEvidenceForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto tracked_ad_evidence_it =
      tracked_ad_evidence_.find(render_frame_host->GetFrameTreeNodeId());
  if (tracked_ad_evidence_it == tracked_ad_evidence_.end())
    return absl::nullopt;
  return tracked_ad_evidence_it->second;
}

void ContentSubresourceFilterThrottleManager::DidDisallowFirstSubresource() {
  MaybeShowNotification(receiver_.GetCurrentTargetFrame());
}

void ContentSubresourceFilterThrottleManager::FrameIsAd() {
  // `FrameIsAd()` can only be called for an initial empty document. As it won't
  // pass through `ReadyToCommitNavigation()` (and has not yet passed through
  // `DidFinishNavigation()`), we know it won't be updated further.
  //
  // The fenced frame root will not report this for the initial empty document
  // as the fenced frame is isolated from the embedder cannot be scripted. In
  // that case we do can rely on passing through ReadyToCommitNavigation and we
  // do so since fenced frame ad tagging varies significantly from regular
  // iframes, see `AdScriptDidCreateFencedFrame`.
  content::RenderFrameHost* render_frame_host =
      receiver_.GetCurrentTargetFrame();
  DCHECK(!render_frame_host->IsFencedFrameRoot());
  OnFrameIsAd(receiver_.GetCurrentTargetFrame());
}

void ContentSubresourceFilterThrottleManager::SetDocumentLoadStatistics(
    mojom::DocumentLoadStatisticsPtr statistics) {
  if (statistics_)
    statistics_->OnDocumentLoadStatistics(*statistics);
}

void ContentSubresourceFilterThrottleManager::OnAdsViolationTriggered(
    mojom::AdsViolation violation) {
  DCHECK(page_);
  DCHECK_EQ(&GetSubresourceFilterRootPage(receiver_.GetCurrentTargetFrame()),
            page_);
  // TODO(bokan) How should we deal with violations coming from a fenced frame?
  // Should we pass in the fenced frame root instead? crbug.com/1263541.
  OnAdsViolationTriggered(&page_->GetMainDocument(), violation);
}

void ContentSubresourceFilterThrottleManager::FrameWasCreatedByAdScript() {
  OnChildFrameWasCreatedByAdScript(receiver_.GetCurrentTargetFrame());
}

void ContentSubresourceFilterThrottleManager::AdScriptDidCreateFencedFrame(
    const blink::RemoteFrameToken& placeholder_token) {
  if (!blink::features::IsFencedFramesEnabled()) {
    mojo::ReportBadMessage(
        "AdScriptDidCreateFencedFrame can only be called when fenced frames "
        "are enabled.");
    return;
  }

  // This method is called from the renderer when it creates a new MPArch-based
  // fenced frame while an ad script was on the v8 stack.
  //
  // Normal frames compute this when the new RenderFrame initializes since that
  // happens synchronously during the CreateFrame call. At that time,
  // SubresourceFilterAgent calls FrameWasCreatedByAdScript if needed.  However,
  // creating an MPArch-based FencedFrame doesn't create a RenderFrame in the
  // calling process; it creates a RenderFrame in another renderer via IPC at
  // which point we cannot inspect the v8 stack so we use this special path for
  // fenced frames.

  content::RenderFrameHost* owner_frame = receiver_.GetCurrentTargetFrame();
  DCHECK(owner_frame);

  auto* fenced_frame_root = content::RenderFrameHost::FromPlaceholderToken(
      owner_frame->GetProcess()->GetID(), placeholder_token);

  if (!fenced_frame_root)
    return;

  if (!fenced_frame_root->IsFencedFrameRoot()) {
    mojo::ReportBadMessage(
        "AdScriptDidCreateFencedFrame received token for frame that isn't a "
        "fenced frame root.");
    return;
  }

  if (fenced_frame_root->GetParentOrOuterDocument() != owner_frame) {
    mojo::ReportBadMessage(
        "AdScriptDidCreateFencedFrame called from non-embedder of fenced "
        "frame.");
    return;
  }

  DCHECK(!base::Contains(tracked_ad_evidence_,
                         fenced_frame_root->GetFrameTreeNodeId()));
  OnChildFrameWasCreatedByAdScript(fenced_frame_root);
}

void ContentSubresourceFilterThrottleManager::OnChildFrameWasCreatedByAdScript(
    content::RenderFrameHost* frame_host) {
  DCHECK(frame_host);
  DCHECK(!IsSubresourceFilterRoot(frame_host));

  EnsureFrameAdEvidence(frame_host)
      .set_created_by_ad_script(
          blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
}

blink::FrameAdEvidence&
ContentSubresourceFilterThrottleManager::EnsureFrameAdEvidence(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!IsInSubresourceFilterRoot(navigation_handle));
  auto frame_tree_node_id = navigation_handle->GetFrameTreeNodeId();
  auto parent_frame_tree_node_id =
      navigation_handle->GetParentFrameOrOuterDocument()->GetFrameTreeNodeId();
  return EnsureFrameAdEvidence(frame_tree_node_id, parent_frame_tree_node_id);
}

blink::FrameAdEvidence&
ContentSubresourceFilterThrottleManager::EnsureFrameAdEvidence(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(!IsSubresourceFilterRoot(render_frame_host));
  auto frame_tree_node_id = render_frame_host->GetFrameTreeNodeId();
  auto parent_frame_tree_node_id =
      render_frame_host->GetParentOrOuterDocument()->GetFrameTreeNodeId();
  return EnsureFrameAdEvidence(frame_tree_node_id, parent_frame_tree_node_id);
}

blink::FrameAdEvidence&
ContentSubresourceFilterThrottleManager::EnsureFrameAdEvidence(
    int frame_tree_node_id,
    int parent_frame_tree_node_id) {
  DCHECK_NE(frame_tree_node_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  DCHECK_NE(parent_frame_tree_node_id,
            content::RenderFrameHost::kNoFrameTreeNodeId);
  return tracked_ad_evidence_
      .emplace(frame_tree_node_id,
               /*parent_is_ad=*/base::Contains(ad_frames_,
                                               parent_frame_tree_node_id))
      .first->second;
}

}  // namespace subresource_filter

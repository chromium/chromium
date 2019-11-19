// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subframe_navigation_filtering_throttle.h"

#include <sstream>

#include "base/bind.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace subresource_filter {

SubframeNavigationFilteringThrottle::SubframeNavigationFilteringThrottle(
    content::NavigationHandle* handle,
    AsyncDocumentSubresourceFilter* parent_frame_filter,
    Delegate* delegate)
    : content::NavigationThrottle(handle),
      parent_frame_filter_(parent_frame_filter),
      delegate_(delegate) {
  DCHECK(!handle->IsInMainFrame());
  DCHECK(parent_frame_filter_);
}

SubframeNavigationFilteringThrottle::~SubframeNavigationFilteringThrottle() {
  switch (load_policy_) {
    case LoadPolicy::ALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Allowed",
          total_defer_time_, base::TimeDelta::FromMicroseconds(1),
          base::TimeDelta::FromSeconds(10), 50);
      break;
    case LoadPolicy::WOULD_DISALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.WouldDisallow",
          total_defer_time_, base::TimeDelta::FromMicroseconds(1),
          base::TimeDelta::FromSeconds(10), 50);
      break;
    case LoadPolicy::DISALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Disallowed2",
          total_defer_time_, base::TimeDelta::FromMicroseconds(1),
          base::TimeDelta::FromSeconds(10), 50);
      break;
  }
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillStartRequest() {
  return MaybeDeferToCalculateLoadPolicy();
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillRedirectRequest() {
  return MaybeDeferToCalculateLoadPolicy();
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillProcessResponse() {
  DCHECK_NE(load_policy_, LoadPolicy::DISALLOW);

  // Load policy notifications should go out by WillProcessResponse,
  // defer if we are still performing any ruleset checks. If we are here,
  // and there are outstanding load policy calculations, we are in dry run
  // mode.
  if (pending_load_policy_calculations_ > 0) {
    DCHECK((parent_frame_filter_->activation_state().activation_level ==
            mojom::ActivationLevel::kDryRun));
    DeferStart(DeferStage::kWillProcessResponse);
    return DEFER;
  }

  NotifyLoadPolicy();
  return PROCEED;
}

const char* SubframeNavigationFilteringThrottle::GetNameForLogging() {
  return "SubframeNavigationFilteringThrottle";
}

void SubframeNavigationFilteringThrottle::HandleDisallowedLoad() {
  if (parent_frame_filter_->activation_state().enable_logging) {
    std::string console_message = base::StringPrintf(
        kDisallowSubframeConsoleMessageFormat,
        navigation_handle()->GetURL().possibly_invalid_spec().c_str());
    navigation_handle()->GetWebContents()->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError, console_message);
  }

  parent_frame_filter_->ReportDisallowedLoad();
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::MaybeDeferToCalculateLoadPolicy() {
  DCHECK_NE(load_policy_, LoadPolicy::DISALLOW);
  if (load_policy_ == LoadPolicy::WOULD_DISALLOW)
    return PROCEED;

  pending_load_policy_calculations_ += 1;
  parent_frame_filter_->GetLoadPolicyForSubdocument(
      navigation_handle()->GetURL(),
      base::BindOnce(
          &SubframeNavigationFilteringThrottle::OnCalculatedLoadPolicy,
          weak_ptr_factory_.GetWeakPtr()));

  // If the embedder document has activation enabled, we calculate frame load
  // policy before proceeding with navigation as filtered navigations are not
  // allowed to get a response. As a result, we must defer while
  // we wait for the ruleset check to complete and pass handling the navigation
  // decision to the callback.
  if (parent_frame_filter_->activation_state().activation_level ==
      mojom::ActivationLevel::kEnabled) {
    DeferStart(DeferStage::kWillStartOrRedirectRequest);
    return DEFER;
  }

  // Otherwise, issue the ruleset request in parallel as an optimization.
  return PROCEED;
}

void SubframeNavigationFilteringThrottle::OnCalculatedLoadPolicy(
    LoadPolicy policy) {
  load_policy_ = MoreRestrictiveLoadPolicy(policy, load_policy_);
  pending_load_policy_calculations_ -= 1;

  // Callback is not responsible for handling navigation if we are not deferred.
  if (defer_stage_ == DeferStage::kNotDeferring)
    return;

  // When we are deferred, callback is not responsible for handling navigation
  // if there are still outstanding load policy calculations.
  if (pending_load_policy_calculations_ > 0) {
    // We defer waiting for each load policy calculations when the embedder
    // document has activation enabled.
    DCHECK(parent_frame_filter_->activation_state().activation_level !=
           mojom::ActivationLevel::kEnabled);
    return;
  }

  // If we are deferred and there are no pending load policy calculations,
  // handle the deferred navigation.
  DCHECK(defer_stage_ == DeferStage::kWillProcessResponse ||
         defer_stage_ == DeferStage::kWillStartOrRedirectRequest);
  DCHECK(!last_defer_timestamp_.is_null());
  bool deferring_response = defer_stage_ == DeferStage::kWillProcessResponse;
  total_defer_time_ += base::TimeTicks::Now() - last_defer_timestamp_;
  defer_stage_ = DeferStage::kNotDeferring;
  if (deferring_response) {
    NotifyLoadPolicy();
    Resume();
    return;
  }

  // Otherwise, we deferred at start/redirect time. Either cancel navigation
  // or resume here according to load policy.
  if (load_policy_ == LoadPolicy::DISALLOW) {
    HandleDisallowedLoad();
    NotifyLoadPolicy();
    CancelDeferredNavigation(BLOCK_REQUEST_AND_COLLAPSE);
  } else {
    Resume();
  }
}

void SubframeNavigationFilteringThrottle::DeferStart(DeferStage stage) {
  DCHECK(defer_stage_ == DeferStage::kNotDeferring);
  DCHECK(stage != DeferStage::kNotDeferring);
  defer_stage_ = stage;
  last_defer_timestamp_ = base::TimeTicks::Now();
}

void SubframeNavigationFilteringThrottle::NotifyLoadPolicy() const {
  auto* observer_manager = SubresourceFilterObserverManager::FromWebContents(
      navigation_handle()->GetWebContents());
  if (!observer_manager)
    return;

  content::GlobalFrameRoutingId starting_rfh_id =
      navigation_handle()->GetPreviousRenderFrameHostId();
  content::RenderFrameHost* starting_rfh = content::RenderFrameHost::FromID(
      starting_rfh_id.child_id, starting_rfh_id.frame_routing_id);

  // |starting_rfh| can be null if the navigation started from a non live
  // RenderFrameHost. For instance when a renderer process crashed.
  // See https://crbug.com/904248
  bool is_ad_subframe = starting_rfh && delegate_->CalculateIsAdSubframe(
                                            starting_rfh, load_policy_);

  observer_manager->NotifySubframeNavigationEvaluated(
      navigation_handle(), load_policy_, is_ad_subframe);
}

}  // namespace subresource_filter

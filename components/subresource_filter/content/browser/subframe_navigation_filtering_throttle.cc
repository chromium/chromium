// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subframe_navigation_filtering_throttle.h"

#include <sstream>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/console_message_level.h"

namespace subresource_filter {

SubframeNavigationFilteringThrottle::SubframeNavigationFilteringThrottle(
    content::NavigationHandle* handle,
    AsyncDocumentSubresourceFilter* parent_frame_filter,
    Delegate* delegate)
    : content::NavigationThrottle(handle),
      parent_frame_filter_(parent_frame_filter),
      delegate_(delegate),
      weak_ptr_factory_(this) {
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
    // fall through
    case LoadPolicy::DISALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Disallowed",
          total_defer_time_, base::TimeDelta::FromMicroseconds(1),
          base::TimeDelta::FromSeconds(10), 50);
      break;
  }
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillStartRequest() {
  return DeferToCalculateLoadPolicy();
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillRedirectRequest() {
  return DeferToCalculateLoadPolicy();
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::WillProcessResponse() {
  DCHECK_NE(load_policy_, LoadPolicy::DISALLOW);
  NotifyLoadPolicy();
  return PROCEED;
}

const char* SubframeNavigationFilteringThrottle::GetNameForLogging() {
  return "SubframeNavigationFilteringThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
SubframeNavigationFilteringThrottle::DeferToCalculateLoadPolicy() {
  DCHECK_NE(load_policy_, LoadPolicy::DISALLOW);
  if (load_policy_ == LoadPolicy::WOULD_DISALLOW)
    return PROCEED;
  parent_frame_filter_->GetLoadPolicyForSubdocument(
      navigation_handle()->GetURL(),
      base::BindOnce(
          &SubframeNavigationFilteringThrottle::OnCalculatedLoadPolicy,
          weak_ptr_factory_.GetWeakPtr()));
  last_defer_timestamp_ = base::TimeTicks::Now();
  return DEFER;
}

void SubframeNavigationFilteringThrottle::OnCalculatedLoadPolicy(
    LoadPolicy policy) {
  DCHECK(!last_defer_timestamp_.is_null());
  load_policy_ = policy;
  total_defer_time_ += base::TimeTicks::Now() - last_defer_timestamp_;

  if (policy == LoadPolicy::DISALLOW) {
    if (parent_frame_filter_->activation_state().enable_logging) {
      std::string console_message = base::StringPrintf(
          kDisallowSubframeConsoleMessageFormat,
          navigation_handle()->GetURL().possibly_invalid_spec().c_str());
      navigation_handle()
          ->GetWebContents()
          ->GetMainFrame()
          ->AddMessageToConsole(content::CONSOLE_MESSAGE_LEVEL_ERROR,
                                console_message);
    }

    parent_frame_filter_->ReportDisallowedLoad();
    // Other load policies will be reported in WillProcessResponse.
    NotifyLoadPolicy();

    CancelDeferredNavigation(BLOCK_REQUEST_AND_COLLAPSE);
  } else {
    Resume();
  }
}

void SubframeNavigationFilteringThrottle::NotifyLoadPolicy() const {
  auto* observer_manager = SubresourceFilterObserverManager::FromWebContents(
      navigation_handle()->GetWebContents());
  if (!observer_manager)
    return;

  // TODO(crbug.com/843646): Use an API that NavigationHandle supports rather
  // than trying to infer what the NavigationHandle is doing.
  content::RenderFrameHost* starting_rfh =
      navigation_handle()->GetWebContents()->UnsafeFindFrameByFrameTreeNodeId(
          navigation_handle()->GetFrameTreeNodeId());
  DCHECK(starting_rfh);

  bool is_ad_subframe =
      delegate_->CalculateIsAdSubframe(starting_rfh, load_policy_);

  observer_manager->NotifySubframeNavigationEvaluated(
      navigation_handle(), load_policy_, is_ad_subframe);
}

}  // namespace subresource_filter

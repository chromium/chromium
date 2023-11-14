// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/child_frame_navigation_filtering_throttle.h"

#include <sstream>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "components/subresource_filter/content/common/subresource_filter_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace features {

// Enables or disables performing SubresourceFilter checks from the Browser
// against any aliases for the requested URL found from DNS CNAME records.
BASE_FEATURE(kSendCnameAliasesToSubresourceFilterFromBrowser,
             "SendCnameAliasesToSubresourceFilterFromBrowser",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace subresource_filter {

ChildFrameNavigationFilteringThrottle::ChildFrameNavigationFilteringThrottle(
    content::NavigationHandle* handle,
    AsyncDocumentSubresourceFilter* parent_frame_filter,
    blink::FrameAdEvidence ad_evidence)
    : content::NavigationThrottle(handle),
      parent_frame_filter_(parent_frame_filter),
      alias_check_enabled_(base::FeatureList::IsEnabled(
          ::features::kSendCnameAliasesToSubresourceFilterFromBrowser)),
      ad_evidence_(std::move(ad_evidence)) {
  DCHECK(!IsInSubresourceFilterRoot(handle));
  DCHECK(parent_frame_filter_);
  // Complete the ad evidence as it will be used to make best-effort tagging
  // decisions by request time for ongoing subframe navs.
  ad_evidence_.set_is_complete();
}

ChildFrameNavigationFilteringThrottle::
    ~ChildFrameNavigationFilteringThrottle() {
  switch (load_policy_) {
    case LoadPolicy::EXPLICITLY_ALLOW:
      [[fallthrough]];
    case LoadPolicy::ALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Allowed",
          total_defer_time_, base::Microseconds(1), base::Seconds(10), 50);
      break;
    case LoadPolicy::WOULD_DISALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.WouldDisallow",
          total_defer_time_, base::Microseconds(1), base::Seconds(10), 50);
      break;
    case LoadPolicy::DISALLOW:
      UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
          "SubresourceFilter.DocumentLoad.SubframeFilteringDelay.Disallowed2",
          total_defer_time_, base::Microseconds(1), base::Seconds(10), 50);
      break;
  }
}

content::NavigationThrottle::ThrottleCheckResult
ChildFrameNavigationFilteringThrottle::WillStartRequest() {
  return MaybeDeferToCalculateLoadPolicy();
}

content::NavigationThrottle::ThrottleCheckResult
ChildFrameNavigationFilteringThrottle::WillRedirectRequest() {
  return MaybeDeferToCalculateLoadPolicy();
}

content::NavigationThrottle::ThrottleCheckResult
ChildFrameNavigationFilteringThrottle::WillProcessResponse() {
  DCHECK_NE(load_policy_, LoadPolicy::DISALLOW);

  if (alias_check_enabled_) {
    std::vector<GURL> alias_urls;
    const GURL& base_url = navigation_handle()->GetURL();

    for (const auto& alias : navigation_handle()->GetDnsAliases()) {
      if (alias == navigation_handle()->GetURL().host_piece()) {
        continue;
      }

      GURL::Replacements replacements;
      replacements.SetHostStr(alias);
      GURL alias_url = base_url.ReplaceComponents(replacements);

      if (alias_url.is_valid()) {
        alias_urls.push_back(alias_url);
      }
    }

    if (!alias_urls.empty()) {
      pending_load_policy_calculations_++;
      parent_frame_filter_->GetLoadPolicyForSubdocumentURLs(
          alias_urls, base::BindOnce(&ChildFrameNavigationFilteringThrottle::
                                         OnCalculatedLoadPoliciesFromAliasUrls,
                                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // Load policy notifications should go out by WillProcessResponse, unless
  // we received CNAME aliases in the response and alias checking is enabled.
  // Defer if we are still performing any ruleset checks. If we are here,
  // and there are outstanding load policy calculations, we are either in dry
  // run mode or checking aliases.
  if (pending_load_policy_calculations_ > 0) {
    DCHECK(parent_frame_filter_->activation_state().activation_level ==
               mojom::ActivationLevel::kDryRun ||
           navigation_handle()->GetDnsAliases().size() > 0);
    DeferStart(DeferStage::kWillProcessResponse);
    return DEFER;
  }

  NotifyLoadPolicy();
  return PROCEED;
}

const char* ChildFrameNavigationFilteringThrottle::GetNameForLogging() {
  return "ChildFrameNavigationFilteringThrottle";
}

void ChildFrameNavigationFilteringThrottle::HandleDisallowedLoad() {
  if (parent_frame_filter_->activation_state().enable_logging) {
    std::string console_message = base::StringPrintf(
        kDisallowChildFrameConsoleMessageFormat,
        navigation_handle()->GetURL().possibly_invalid_spec().c_str());
    // Use the parent's Page to log a message to the console so that if this
    // frame is the root of a nested frame tree (e.g. fenced frame), the log
    // message won't be associated with a to-be-destroyed Page.
    navigation_handle()
        ->GetParentFrameOrOuterDocument()
        ->GetPage()
        .GetMainDocument()
        .AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kError,
                             console_message);
  }

  parent_frame_filter_->ReportDisallowedLoad();
}

content::NavigationThrottle::ThrottleCheckResult
ChildFrameNavigationFilteringThrottle::MaybeDeferToCalculateLoadPolicy() {
  DCHECK_NE(load_policy_, LoadPolicy::DISALLOW);
  if (load_policy_ == LoadPolicy::WOULD_DISALLOW) {
    return PROCEED;
  }

  pending_load_policy_calculations_ += 1;
  parent_frame_filter_->GetLoadPolicyForSubdocument(
      navigation_handle()->GetURL(),
      base::BindOnce(
          &ChildFrameNavigationFilteringThrottle::OnCalculatedLoadPolicy,
          weak_ptr_factory_.GetWeakPtr()));

  // If the embedder document has activation enabled, we calculate frame load
  // policy before proceeding with navigation as filtered navigations are not
  // allowed to get a response. As a result, we must defer while
  // we wait for the ruleset check to complete and pass handling the navigation
  // decision to the callback.
  //
  // If `kTPCDAdHeuristicSubframeRequestTagging`, we always need to defer
  // navigation start to ensure we have the load policy calculated in order
  // to properly tag the navigation handle as an ad before it goes to the
  // network.
  if (parent_frame_filter_->activation_state().activation_level ==
          mojom::ActivationLevel::kEnabled ||
      base::FeatureList::IsEnabled(kTPCDAdHeuristicSubframeRequestTagging)) {
    DeferStart(DeferStage::kWillStartOrRedirectRequest);
    return DEFER;
  }

  // Otherwise, issue the ruleset request in parallel as an optimization.
  return PROCEED;
}

void ChildFrameNavigationFilteringThrottle::OnCalculatedLoadPolicy(
    LoadPolicy policy) {
  // TODO(https://crbug.com/1046806): Modify this call in cases where the new
  // |policy| matches an explicitly allowed rule, rather than using the most
  // restrictive policy for the redirect chain.
  load_policy_ = MoreRestrictiveLoadPolicy(policy, load_policy_);
  pending_load_policy_calculations_ -= 1;

  // Callback is not responsible for handling navigation if we are not deferred.
  if (defer_stage_ == DeferStage::kNotDeferring) {
    return;
  }

  DCHECK(defer_stage_ == DeferStage::kWillProcessResponse ||
         defer_stage_ == DeferStage::kWillStartOrRedirectRequest);

  // If we have an activation enabled and `load_policy_` is DISALLOW, we need
  // to cancel the navigation.
  if (parent_frame_filter_->activation_state().activation_level ==
          mojom::ActivationLevel::kEnabled &&
      load_policy_ == LoadPolicy::DISALLOW) {
    CancelNavigation();
    return;
  }

  // If there are still pending load calculations, then don't resume.
  if (pending_load_policy_calculations_ > 0) {
    return;
  }

  if (defer_stage_ == DeferStage::kWillStartOrRedirectRequest) {
    // Tag the navigation handle based on the current load policy + evidence
    // before the request starts.
    ad_evidence_.UpdateFilterListResult(
        InterpretLoadPolicyAsEvidence(load_policy_));
    if (ad_evidence_.IndicatesAdFrame()) {
      navigation_handle()->SetIsAdTagged();
    }
  }

  ResumeNavigation();
}

void ChildFrameNavigationFilteringThrottle::
    OnCalculatedLoadPoliciesFromAliasUrls(std::vector<LoadPolicy> policies) {
  // We deferred to check aliases in WillProcessResponse.
  DCHECK(defer_stage_ == DeferStage::kWillProcessResponse);
  DCHECK(!policies.empty());

  LoadPolicy most_restricive_alias_policy = LoadPolicy::EXPLICITLY_ALLOW;

  for (LoadPolicy policy : policies) {
    most_restricive_alias_policy =
        MoreRestrictiveLoadPolicy(most_restricive_alias_policy, policy);
  }

  OnCalculatedLoadPolicy(most_restricive_alias_policy);
}

void ChildFrameNavigationFilteringThrottle::DeferStart(DeferStage stage) {
  DCHECK(defer_stage_ == DeferStage::kNotDeferring);
  DCHECK(stage != DeferStage::kNotDeferring);
  defer_stage_ = stage;
  last_defer_timestamp_ = base::TimeTicks::Now();
}

void ChildFrameNavigationFilteringThrottle::NotifyLoadPolicy() const {
  auto* observer_manager = SubresourceFilterObserverManager::FromWebContents(
      navigation_handle()->GetWebContents());
  if (!observer_manager) {
    return;
  }

  observer_manager->NotifyChildFrameNavigationEvaluated(navigation_handle(),
                                                        load_policy_);
}

void ChildFrameNavigationFilteringThrottle::UpdateDeferInfo() {
  DCHECK(defer_stage_ != DeferStage::kNotDeferring);
  DCHECK(!last_defer_timestamp_.is_null());
  total_defer_time_ += base::TimeTicks::Now() - last_defer_timestamp_;
  defer_stage_ = DeferStage::kNotDeferring;
}

void ChildFrameNavigationFilteringThrottle::CancelNavigation() {
  bool defer_stage_was_will_process_response =
      defer_stage_ == DeferStage::kWillProcessResponse;

  UpdateDeferInfo();
  HandleDisallowedLoad();
  NotifyLoadPolicy();

  if (defer_stage_was_will_process_response) {
    CancelDeferredNavigation(CANCEL);
  } else {
    CancelDeferredNavigation(BLOCK_REQUEST_AND_COLLAPSE);
  }
}

void ChildFrameNavigationFilteringThrottle::ResumeNavigation() {
  // There are no more pending load calculations. We can toggle back to not
  // being deferred.
  bool defer_stage_was_will_process_response =
      defer_stage_ == DeferStage::kWillProcessResponse;
  UpdateDeferInfo();

  // If the defer stage was WillProcessResponse, then this is the last
  // LoadPolicy that we will calculate.
  if (defer_stage_was_will_process_response) {
    NotifyLoadPolicy();
  }

  Resume();
}

}  // namespace subresource_filter

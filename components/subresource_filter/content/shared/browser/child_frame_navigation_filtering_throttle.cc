// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/browser/child_frame_navigation_filtering_throttle.h"

#include <optional>
#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/stringprintf.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace subresource_filter {

ChildFrameNavigationFilteringThrottle::ChildFrameNavigationFilteringThrottle(
    content::NavigationHandle* handle,
    AsyncDocumentSubresourceFilter* parent_frame_filter,
    bool alias_check_enabled,
    base::RepeatingCallback<std::string(const GURL& url)>
        disallow_message_callback)
    : content::NavigationThrottle(handle),
      parent_frame_filter_(parent_frame_filter),
      alias_check_enabled_(alias_check_enabled),
      disallow_message_callback_(std::move(disallow_message_callback)) {
  CHECK(!IsInSubresourceFilterRoot(handle), base::NotFatalUntil::M129);
  CHECK(parent_frame_filter_, base::NotFatalUntil::M129);
}

ChildFrameNavigationFilteringThrottle::
    ~ChildFrameNavigationFilteringThrottle() = default;

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
  CHECK_NE(load_policy_, LoadPolicy::DISALLOW, base::NotFatalUntil::M129);

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
    CHECK(parent_frame_filter_->activation_state().activation_level ==
                  mojom::ActivationLevel::kDryRun ||
              navigation_handle()->GetDnsAliases().size() > 0,
          base::NotFatalUntil::M129);
    DeferStart(DeferStage::kWillProcessResponse);
    return DEFER;
  }

  NotifyLoadPolicy();
  return PROCEED;
}

void ChildFrameNavigationFilteringThrottle::HandleDisallowedLoad() {
  if (parent_frame_filter_->activation_state().enable_logging) {
    std::string console_message =
        disallow_message_callback_.Run(navigation_handle()->GetURL());

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
  CHECK_NE(load_policy_, LoadPolicy::DISALLOW, base::NotFatalUntil::M129);
  if (load_policy_ == LoadPolicy::WOULD_DISALLOW) {
    return PROCEED;
  }

  pending_load_policy_calculations_ += 1;
  parent_frame_filter_->GetLoadPolicyForSubdocument(
      navigation_handle()->GetURL(),
      base::BindOnce(
          &ChildFrameNavigationFilteringThrottle::OnCalculatedLoadPolicyForUrl,
          weak_ptr_factory_.GetWeakPtr()));

  if (ShouldDeferNavigation()) {
    DeferStart(DeferStage::kWillStartOrRedirectRequest);
    return DEFER;
  }

  // Otherwise, issue the ruleset request in parallel as an optimization.
  return PROCEED;
}

void ChildFrameNavigationFilteringThrottle::OnCalculatedLoadPolicy(
    LoadPolicy policy) {
  // TODO(https://crbug.com/40116607): Modify this call in cases where the new
  // |policy| matches an explicitly allowed rule, rather than using the most
  // restrictive policy for the redirect chain.
  load_policy_ = MoreRestrictiveLoadPolicy(policy, load_policy_);
  pending_load_policy_calculations_ -= 1;

  // Callback is not responsible for handling navigation if we are not deferred.
  if (defer_stage_ == DeferStage::kNotDeferring) {
    return;
  }

  CHECK(defer_stage_ == DeferStage::kWillProcessResponse ||
            defer_stage_ == DeferStage::kWillStartOrRedirectRequest,
        base::NotFatalUntil::M129);

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

  OnReadyToResumeNavigationWithLoadPolicy();
  ResumeNavigation();
}

void ChildFrameNavigationFilteringThrottle::OnCalculatedLoadPolicyForUrl(
    LoadPolicy policy) {
  if (policy != load_policy_ &&
      policy == MoreRestrictiveLoadPolicy(policy, load_policy_)) {
    // Child frame's hostname check determined the load policy.
    did_alias_check_determine_load_policy_ = false;
  }
  OnCalculatedLoadPolicy(policy);
}

void ChildFrameNavigationFilteringThrottle::
    OnCalculatedLoadPoliciesFromAliasUrls(std::vector<LoadPolicy> policies) {
  // We deferred to check aliases in WillProcessResponse.
  CHECK(defer_stage_ == DeferStage::kWillProcessResponse,
        base::NotFatalUntil::M129);
  CHECK(alias_check_enabled_);
  CHECK(!policies.empty(), base::NotFatalUntil::M129);

  did_alias_check_ = true;

  LoadPolicy most_restrictive_alias_policy = LoadPolicy::EXPLICITLY_ALLOW;

  for (LoadPolicy policy : policies) {
    most_restrictive_alias_policy =
        MoreRestrictiveLoadPolicy(most_restrictive_alias_policy, policy);
  }

  if (most_restrictive_alias_policy != load_policy_ &&
      most_restrictive_alias_policy ==
          MoreRestrictiveLoadPolicy(most_restrictive_alias_policy,
                                    load_policy_)) {
    did_alias_check_determine_load_policy_ = true;
  }

  OnCalculatedLoadPolicy(most_restrictive_alias_policy);
}

void ChildFrameNavigationFilteringThrottle::DeferStart(DeferStage stage) {
  CHECK(defer_stage_ == DeferStage::kNotDeferring, base::NotFatalUntil::M129);
  CHECK(stage != DeferStage::kNotDeferring, base::NotFatalUntil::M129);
  defer_stage_ = stage;
  last_defer_timestamp_ = base::TimeTicks::Now();
}

void ChildFrameNavigationFilteringThrottle::UpdateDeferInfo() {
  CHECK(defer_stage_ != DeferStage::kNotDeferring, base::NotFatalUntil::M129);
  CHECK(!last_defer_timestamp_.is_null(), base::NotFatalUntil::M129);
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

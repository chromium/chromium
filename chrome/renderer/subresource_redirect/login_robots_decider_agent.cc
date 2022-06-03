// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/login_robots_decider_agent.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser_cache.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "components/subresource_redirect/common/subresource_redirect_features.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
#include "content/public/renderer/render_frame.h"

namespace subresource_redirect {

namespace {

// Converts the RobotsRulesParser::CheckResult enum to SubresourceRedirectResult
// enum.
SubresourceRedirectResult ConvertToRedirectResult(
    RobotsRulesParser::CheckResult check_result) {
  switch (check_result) {
    case RobotsRulesParser::CheckResult::kAllowed:
      if (ShouldEnableLoginRobotsCheckedImageCompression() &&
          !ShouldCompressRedirectSubresource()) {
        return SubresourceRedirectResult::kIneligibleCompressionDisabled;
      }
      return SubresourceRedirectResult::kRedirectable;
    case RobotsRulesParser::CheckResult::kDisallowed:
    case RobotsRulesParser::CheckResult::kInvalidated:
    case RobotsRulesParser::CheckResult::kEntryMissing:
      return SubresourceRedirectResult::kIneligibleRobotsDisallowed;
    case RobotsRulesParser::CheckResult::kTimedout:
    case RobotsRulesParser::CheckResult::kDisallowedAfterTimeout:
      return SubresourceRedirectResult::kIneligibleRobotsTimeout;
  }
}

void RecordRedirectResultMetric(SubresourceRedirectResult redirect_result) {
  base::UmaHistogramEnumeration(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      redirect_result);
}

}  // namespace

LoginRobotsDeciderAgent::LoginRobotsDeciderAgent(
    blink::AssociatedInterfaceRegistry* associated_interfaces,
    content::RenderFrame* render_frame)
    : PublicResourceDeciderAgent(associated_interfaces, render_frame) {
  DCHECK(ShouldEnableRobotsRulesFetching());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

LoginRobotsDeciderAgent::~LoginRobotsDeciderAgent() = default;

absl::optional<SubresourceRedirectResult>
LoginRobotsDeciderAgent::ShouldRedirectSubresource(
    const GURL& url,
    ShouldRedirectDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  DCHECK(url.is_valid());
  num_should_redirect_checks_++;

  if (redirect_result_ != SubresourceRedirectResult::kRedirectable) {
    RecordRedirectResultMetric(redirect_result_);
    return redirect_result_;
  }

  if (num_should_redirect_checks_ <=
      GetFirstKDisableSubresourceRedirectLimit()) {
    DCHECK_LE(0UL, GetFirstKDisableSubresourceRedirectLimit());
    RecordRedirectResultMetric(
        SubresourceRedirectResult::kIneligibleFirstKDisableSubresourceRedirect);
    return SubresourceRedirectResult::
        kIneligibleFirstKDisableSubresourceRedirect;
  }
  CreateAndFetchRobotsRules(
      url::Origin::Create(url),
      num_should_redirect_checks_ <= GetFirstKSubresourceLimit()
          ? GetRobotsRulesReceiveFirstKSubresourceTimeout()
          : GetRobotsRulesReceiveTimeout());

  absl::optional<RobotsRulesParser::CheckResult> result =
      RobotsRulesParserCache::Get().CheckRobotsRules(
          routing_id(), url,
          base::BindOnce(
              &LoginRobotsDeciderAgent::OnShouldRedirectSubresourceResult,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  if (result) {
    SubresourceRedirectResult redirect_result =
        ConvertToRedirectResult(*result);
    RecordRedirectResultMetric(redirect_result);
    return redirect_result;
  }

  return absl::nullopt;
}

void LoginRobotsDeciderAgent::OnShouldRedirectSubresourceResult(
    LoginRobotsDeciderAgent::ShouldRedirectDecisionCallback callback,
    RobotsRulesParser::CheckResult check_result) {
  // Verify if the navigation is still allowed to redirect.
  if (redirect_result_ != SubresourceRedirectResult::kRedirectable) {
    RecordRedirectResultMetric(redirect_result_);
    std::move(callback).Run(redirect_result_);
    return;
  }
  SubresourceRedirectResult redirect_result =
      ConvertToRedirectResult(check_result);
  RecordRedirectResultMetric(redirect_result);
  std::move(callback).Run(redirect_result);
}

void LoginRobotsDeciderAgent::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PublicResourceDeciderAgent::ReadyToCommitNavigation(document_loader);
  if (is_pending_navigation_loggged_in_) {
    redirect_result_ = *is_pending_navigation_loggged_in_
                           ? SubresourceRedirectResult::kIneligibleLoginDetected
                           : SubresourceRedirectResult::kRedirectable;
    // Clear the logged-in state so it won't be reused for subsequent
    // navigations.
    is_pending_navigation_loggged_in_ = absl::nullopt;
  } else {
    // Logged-in state was not sent for the current navigation.
    redirect_result_ = SubresourceRedirectResult::kUnknown;
  }
  num_should_redirect_checks_ = 0;
  // Invalidate the previous requests that were started by previous navigation,
  // for the current frame.
  RobotsRulesParserCache::Get().InvalidatePendingRequests(routing_id());
}

void LoginRobotsDeciderAgent::SetLoggedInState(bool is_logged_in) {
  // Logged-in state is sent when a new navigation is about to commit in the
  // browser process. Save this state until the navigation is committed in the
  // renderer process.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_pending_navigation_loggged_in_);
  is_pending_navigation_loggged_in_ = is_logged_in;
}

void LoginRobotsDeciderAgent::RecordMetricsOnLoadFinished(
    const GURL& url,
    int64_t content_length,
    SubresourceRedirectResult redirect_result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(crbug.com/1148980): Record coverage metrics
}

void LoginRobotsDeciderAgent::SetCompressPublicImagesHints(
    mojom::CompressPublicImagesHintsPtr images_hints) {
  // This mojo from browser process should not be called for robots rules based
  // subresource compression on non logged-in pages.
  DCHECK(ShouldEnableRobotsRulesFetching());
  NOTREACHED();
}

void LoginRobotsDeciderAgent::NotifyIneligibleBlinkDisallowedSubresource() {
  num_should_redirect_checks_++;
}

void LoginRobotsDeciderAgent::CreateAndFetchRobotsRules(
    const url::Origin& origin,
    const base::TimeDelta& rules_receive_timeout) {
  DCHECK(!origin.opaque());
  RobotsRulesParserCache& robots_rules_parser_cache =
      RobotsRulesParserCache::Get();
  if (!robots_rules_parser_cache.DoRobotsRulesParserExist(origin)) {
    // Create the robots rules parser and start the fetch as well.
    robots_rules_parser_cache.CreateRobotsRulesParser(origin,
                                                      rules_receive_timeout);
    GetSubresourceRedirectServiceRemote()->GetRobotsRules(
        origin, base::BindOnce(&RobotsRulesParserCache::UpdateRobotsRules,
                               robots_rules_parser_cache.GetWeakPtr(), origin));
  }
}

void LoginRobotsDeciderAgent::PreloadSubresourceOptimizationsForOrigins(
    const std::vector<blink::WebSecurityOrigin>& origins) {
  for (const auto& origin : origins) {
    CreateAndFetchRobotsRules(origin, GetRobotsRulesReceiveTimeout());
  }
}

}  // namespace subresource_redirect

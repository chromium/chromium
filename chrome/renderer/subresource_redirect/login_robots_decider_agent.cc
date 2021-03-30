// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/login_robots_decider_agent.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser_cache.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "components/subresource_redirect/common/subresource_redirect_features.h"
#include "components/subresource_redirect/common/subresource_redirect_result.h"
#include "content/public/renderer/render_frame.h"

namespace subresource_redirect {

namespace {

// Returns the robots rules parser cache that is shared across the RenderFrames
// in the renderer.
RobotsRulesParserCache& GetRobotsRulesParserCache() {
  static base::NoDestructor<RobotsRulesParserCache> instance;
  return *instance;
}

// Converts the RobotsRulesParser::CheckResult enum to SubresourceRedirectResult
// enum.
SubresourceRedirectResult ConvertToRedirectResult(
    RobotsRulesParser::CheckResult check_result) {
  switch (check_result) {
    case RobotsRulesParser::CheckResult::kAllowed:
      return SubresourceRedirectResult::kRedirectable;
    case RobotsRulesParser::CheckResult::kDisallowed:
    case RobotsRulesParser::CheckResult::kInvalidated:
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

base::Optional<SubresourceRedirectResult>
LoginRobotsDeciderAgent::ShouldRedirectSubresource(
    const GURL& url,
    ShouldRedirectDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(url.is_valid());
  num_should_redirect_checks_++;

  if (redirect_result_ != SubresourceRedirectResult::kRedirectable) {
    RecordRedirectResultMetric(redirect_result_);
    return redirect_result_;
  }

  // Trigger the robots rules fetch if needed.
  const auto origin = url::Origin::Create(url);
  RobotsRulesParserCache& robots_rules_parser_cache =
      GetRobotsRulesParserCache();
  if (!robots_rules_parser_cache.DoRobotsRulesExist(origin)) {
    // base::Unretained can be used here since the |robots_rules_parser_cache|
    // is never destructed.
    GetSubresourceRedirectServiceRemote()->GetRobotsRules(
        origin,
        base::BindOnce(&RobotsRulesParserCache::UpdateRobotsRules,
                       base::Unretained(&robots_rules_parser_cache), origin));
  }
  auto rules_receive_timeout =
      num_should_redirect_checks_ <= GetFirstKSubresourceLimit()
          ? GetRobotsRulesReceiveFirstKSubresourceTimeout()
          : GetRobotsRulesReceiveTimeout();

  base::Optional<RobotsRulesParser::CheckResult> result =
      robots_rules_parser_cache.CheckRobotsRules(
          routing_id(), url, rules_receive_timeout,
          base::BindOnce(
              &LoginRobotsDeciderAgent::OnShouldRedirectSubresourceResult,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  if (result) {
    SubresourceRedirectResult redirect_result =
        ConvertToRedirectResult(*result);
    RecordRedirectResultMetric(redirect_result);
    return redirect_result;
  }

  return base::nullopt;
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
  redirect_result_ = SubresourceRedirectResult::kUnknown;
  num_should_redirect_checks_ = 0;
  GetRobotsRulesParserCache().InvalidatePendingRequests(routing_id());
}

void LoginRobotsDeciderAgent::SetLoggedInState(bool is_logged_in) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  redirect_result_ = is_logged_in
                         ? SubresourceRedirectResult::kIneligibleLoginDetected
                         : SubresourceRedirectResult::kRedirectable;
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

void LoginRobotsDeciderAgent::UpdateRobotsRulesForTesting(
    const url::Origin& origin,
    const base::Optional<std::string>& rules) {
  GetRobotsRulesParserCache().UpdateRobotsRules(origin, rules);
}

}  // namespace subresource_redirect

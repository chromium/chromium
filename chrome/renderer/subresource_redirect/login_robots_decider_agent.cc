// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/login_robots_decider_agent.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "chrome/renderer/subresource_redirect/redirect_result.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"
#include "chrome/renderer/subresource_redirect/robots_rules_parser_cache.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "content/public/renderer/render_frame.h"

namespace subresource_redirect {

namespace {

// Returns the robots rules parser cache that is shared across the RenderFrames
// in the renderer.
RobotsRulesParserCache& GetRobotsRulesParserCache() {
  static base::NoDestructor<RobotsRulesParserCache> instance;
  return *instance;
}

// Converts the RobotsRulesParser::CheckResult enum to RedirectResult enum.
RedirectResult ConvertToRedirectResult(
    RobotsRulesParser::CheckResult check_result) {
  switch (check_result) {
    case RobotsRulesParser::CheckResult::kAllowed:
      return RedirectResult::kRedirectable;
    case RobotsRulesParser::CheckResult::kDisallowed:
      return RedirectResult::kIneligibleRobotsDisallowed;
    case RobotsRulesParser::CheckResult::kTimedout:
    case RobotsRulesParser::CheckResult::kDisallowedAfterTimeout:
      return RedirectResult::kIneligibleRobotsTimeout;
  }
}

// Converts the robots rules CheckResult to RedirectResult and passes to the
// callback.
void SendRedirectResultToCallback(
    LoginRobotsDeciderAgent::ShouldRedirectDecisionCallback callback,
    RobotsRulesParser::CheckResult check_result) {
  std::move(callback).Run(ConvertToRedirectResult(check_result));
}

}  // namespace

LoginRobotsDeciderAgent::LoginRobotsDeciderAgent(
    blink::AssociatedInterfaceRegistry* associated_interfaces,
    content::RenderFrame* render_frame)
    : PublicResourceDeciderAgent(associated_interfaces, render_frame) {
  DCHECK(IsLoginRobotsCheckedCompressionEnabled());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

LoginRobotsDeciderAgent::~LoginRobotsDeciderAgent() = default;

base::Optional<RedirectResult>
LoginRobotsDeciderAgent::ShouldRedirectSubresource(
    const GURL& url,
    ShouldRedirectDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(url.is_valid());
  if (!render_frame()->IsMainFrame())
    return RedirectResult::kIneligibleSubframeResource;

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

  base::Optional<RobotsRulesParser::CheckResult> result =
      robots_rules_parser_cache.CheckRobotsRules(
          url,
          base::BindOnce(&SendRedirectResultToCallback, std::move(callback)));
  if (result)
    return ConvertToRedirectResult(*result);

  return base::nullopt;
}

void LoginRobotsDeciderAgent::RecordMetricsOnLoadFinished(
    const GURL& url,
    int64_t content_length,
    RedirectResult redirect_result) {
  LOCAL_HISTOGRAM_ENUMERATION(
      "SubresourceRedirect.LoginRobotsDeciderAgent.RedirectResult",
      redirect_result);
  // TODO(crbug.com/1148980): Record coverage metrics
}

void LoginRobotsDeciderAgent::SetCompressPublicImagesHints(
    mojom::CompressPublicImagesHintsPtr images_hints) {
  // This mojo from browser process should not be called for robots rules based
  // subresource compression on non logged-in pages.
  DCHECK(IsLoginRobotsCheckedCompressionEnabled());
  NOTREACHED();
}

void LoginRobotsDeciderAgent::UpdateRobotsRulesForTesting(
    const url::Origin& origin,
    const base::Optional<std::string>& rules) {
  GetRobotsRulesParserCache().UpdateRobotsRules(origin, rules);
}

}  // namespace subresource_redirect

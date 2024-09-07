// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_navigation_throttle.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/content/safe_sites_navigation_throttle.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

using URLBlocklistState = policy::URLBlocklist::URLBlocklistState;
using SafeSitesFilterBehavior = policy::SafeSitesFilterBehavior;

// Passing an Unretained pointer for the safe_sites_navigation_throttle_
// callback is safe because this object owns safe_sites_navigation_throttle_,
// which runs the callback from within the object.
PolicyBlocklistNavigationThrottle::PolicyBlocklistNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    content::BrowserContext* context)
    : content::NavigationThrottle(navigation_handle),
      blocklist_service_(PolicyBlocklistFactory::GetForBrowserContext(context)),
      prefs_(user_prefs::UserPrefs::Get(context)) {
  DCHECK(prefs_);
  auto safe_sites_navigation_throttle =
      std::make_unique<SafeSitesNavigationThrottle>(navigation_handle, context);
  if (base::FeatureList::IsEnabled(
          policy::features::kPolicyBlocklistProceedUntilResponse)) {
    safe_sites_navigation_throttle_ =
        std::make_unique<ProceedUntilResponseNavigationThrottle>(
            navigation_handle, std::move(safe_sites_navigation_throttle),
            base::BindRepeating(
                &PolicyBlocklistNavigationThrottle::OnDeferredSafeSitesResult,
                base::Unretained(this)));
  } else {
    safe_sites_navigation_throttle->SetDeferredResultCallback(
        base::BindRepeating(
            &PolicyBlocklistNavigationThrottle::OnDeferredSafeSitesResult,
            base::Unretained(this)));
    safe_sites_navigation_throttle_ = std::move(safe_sites_navigation_throttle);
  }
}

PolicyBlocklistNavigationThrottle::~PolicyBlocklistNavigationThrottle() {
  base::UmaHistogramEnumeration(
      "Navigation.Throttles.PolicyBlocklist.RequestThrottleAction2",
      request_throttle_action_);
  base::UmaHistogramTimes(
      "Navigation.Throttles.PolicyBlocklist.DeferDurationTime",
      defer_duration_);
}

bool PolicyBlocklistNavigationThrottle::IsBlockedViewSourceNavigation() {
  content::NavigationEntry* nav_entry =
      navigation_handle()->GetNavigationEntry();
  if (!nav_entry || !nav_entry->IsViewSourceMode()) {
    return false;
  }

  GURL view_source_url = GURL(std::string("view-source:") +
                              navigation_handle()->GetURL().spec());

  return (blocklist_service_->GetURLBlocklistState(view_source_url) ==
          URLBlocklistState::URL_IN_BLOCKLIST);
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillStartOrRedirectRequest(
    bool is_redirect) {
  if (request_time_.is_null()) {
    request_time_ = base::TimeTicks::Now();
  }
  const GURL& url = navigation_handle()->GetURL();

  // Ignore blob scheme because we may use it to deliver navigation responses
  // to the renderer process.
  if (url.SchemeIs(url::kBlobScheme)) {
    UpdateRequestThrottleAction(PROCEED);
    return PROCEED;
  }

  URLBlocklistState blocklist_state =
      blocklist_service_->GetURLBlocklistState(url);
  if (blocklist_state == URLBlocklistState::URL_IN_BLOCKLIST ||
      IsBlockedViewSourceNavigation()) {
    UpdateRequestThrottleAction(BLOCK_REQUEST);
    return ThrottleCheckResult(BLOCK_REQUEST,
                               net::ERR_BLOCKED_BY_ADMINISTRATOR);
  }

  if (blocklist_state == URLBlocklistState::URL_IN_ALLOWLIST) {
    UpdateRequestThrottleAction(PROCEED);
    return PROCEED;
  }

  ThrottleCheckResult result = CheckSafeSitesFilter(url, is_redirect);
  UpdateRequestThrottleAction(result.action());
  return result;
}

// SafeSitesNavigationThrottle is unconditional and does not check PrefService
// because it is used outside //chrome. Therefore, the policy must be checked
// here to determine whether to use SafeSitesNavigationThrottle.
content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::CheckSafeSitesFilter(const GURL& url,
                                                        bool is_redirect) {
  SafeSitesFilterBehavior filter_behavior =
      static_cast<SafeSitesFilterBehavior>(
          prefs_->GetInteger(policy::policy_prefs::kSafeSitesFilterBehavior));
  if (filter_behavior == SafeSitesFilterBehavior::kSafeSitesFilterDisabled) {
    return PROCEED;
  }

  CHECK_EQ(filter_behavior, SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  return is_redirect ? safe_sites_navigation_throttle_->WillRedirectRequest()
                     : safe_sites_navigation_throttle_->WillStartRequest();
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest(/*is_redirect=*/false);
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest(/*is_redirect=*/true);
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillProcessResponse() {
  base::UmaHistogramTimes(
      "Navigation.Throttles.PolicyBlocklist.RequestToResponseTime",
      request_time_ - base::TimeTicks::Now());
  ThrottleCheckResult result =
      safe_sites_navigation_throttle_->WillProcessResponse();
  UpdateRequestThrottleAction(result.action());
  return result;
}

const char* PolicyBlocklistNavigationThrottle::GetNameForLogging() {
  return "PolicyBlocklistNavigationThrottle";
}

void PolicyBlocklistNavigationThrottle::OnDeferredSafeSitesResult(
    bool proceed,
    std::optional<ThrottleCheckResult> result) {
  defer_duration_ += defer_time_ - base::TimeTicks::Now();
  if (proceed) {
    Resume();
  } else {
    CHECK(result.has_value());
    CancelDeferredNavigation(*result);
  }
}

void PolicyBlocklistNavigationThrottle::UpdateRequestThrottleAction(
    content::NavigationThrottle::ThrottleAction action) {
  switch (action) {
    case PROCEED:
      request_throttle_action_ =
          (request_throttle_action_ == RequestThrottleAction::kNoRequest ||
           request_throttle_action_ == RequestThrottleAction::kProceed)
              ? RequestThrottleAction::kProceed
              : RequestThrottleAction::kProceedAfterDefer;
      break;
    case DEFER:
      request_throttle_action_ = RequestThrottleAction::kDefer;
      defer_time_ = base::TimeTicks::Now();
      break;
    case CANCEL:
    case CANCEL_AND_IGNORE:
    case BLOCK_REQUEST:
    case BLOCK_REQUEST_AND_COLLAPSE:
    case BLOCK_RESPONSE:
      request_throttle_action_ =
          (request_throttle_action_ == RequestThrottleAction::kNoRequest ||
           request_throttle_action_ == RequestThrottleAction::kProceed)
              ? RequestThrottleAction::kBlock
              : RequestThrottleAction::kBlockAfterDefer;
      break;
  }
}

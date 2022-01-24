// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_navigation_throttle.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service_impl.h"
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
    content::BrowserContext* context,
    policy::PolicyService* policy_service)
    : content::NavigationThrottle(navigation_handle),
      safe_sites_navigation_throttle_(
          navigation_handle,
          context,
          base::BindRepeating(
              &PolicyBlocklistNavigationThrottle::OnDeferredSafeSitesResult,
              base::Unretained(this))),
      blocklist_service_(PolicyBlocklistFactory::GetForBrowserContext(context)),
      policy_service_(nullptr),
      prefs_(user_prefs::UserPrefs::Get(context)) {
  if (base::FeatureList::IsEnabled(
          policy::features::kPolicyBlocklistThrottleRequiresPoliciesLoaded)) {
    DCHECK(policy_service);
    policy_service_ = policy_service;
  }
  DCHECK(prefs_);
}

PolicyBlocklistNavigationThrottle::~PolicyBlocklistNavigationThrottle() {
  if (policy_service_)
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
}

bool PolicyBlocklistNavigationThrottle::IsBlockedViewSourceNavigation() {
  content::NavigationEntry* nav_entry =
      navigation_handle()->GetNavigationEntry();
  if (!nav_entry || !nav_entry->IsViewSourceMode())
    return false;

  GURL view_source_url = GURL(std::string("view-source:") +
                              navigation_handle()->GetURL().spec());

  return (blocklist_service_->GetURLBlocklistState(view_source_url) ==
          URLBlocklistState::URL_IN_BLOCKLIST);
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillStartRequest() {
  const GURL& url = navigation_handle()->GetURL();

  // Ignore blob scheme because we may use it to deliver navigation responses
  // to the renderer process.
  if (url.SchemeIs(url::kBlobScheme))
    return PROCEED;

  // Wait for policies to be loaded before checking for blocklists.
  if (policy_service_) {
    // Defer until policies are loaded if there are no pref defines neither
    // UrlBlocklist nor UrlAllowlist and the policies from the Chrome domain
    // have not been loaded yet. Otherwise we assume that we have all the
    // necessary info.
    if (!prefs_->HasPrefPath(policy::policy_prefs::kUrlBlocklist) &&
        !prefs_->HasPrefPath(policy::policy_prefs::kUrlAllowlist) &&
        !policy_service_->IsFirstPolicyLoadComplete(
            policy::POLICY_DOMAIN_CHROME)) {
      // Defer the navigation until policies are loaded. The navigation shall
      // continue after |OnFirstPoliciesLoaded| is called.
      policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
      policy_load_throttle_start_time_ = base::TimeTicks::Now();
      wait_for_policy_timer_.Start(
          FROM_HERE,
          policy::features::kPolicyBlocklistThrottlePolicyLoadTimeout.Get(),
          base::BindOnce(
              &PolicyBlocklistNavigationThrottle::OnFirstPoliciesLoadedTimeout,
              base::Unretained(this)));
      return DEFER;
    }
    // There is no more need to keep a reference the the |policy_service_| since
    // there is no need to wait for policies to load.
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
    policy_service_ = nullptr;
  }

  URLBlocklistState blocklist_state =
      blocklist_service_->GetURLBlocklistState(url);
  if (blocklist_state == URLBlocklistState::URL_IN_BLOCKLIST) {
    return ThrottleCheckResult(BLOCK_REQUEST,
                               net::ERR_BLOCKED_BY_ADMINISTRATOR);
  }

  if (IsBlockedViewSourceNavigation()) {
    return ThrottleCheckResult(BLOCK_REQUEST,
                               net::ERR_BLOCKED_BY_ADMINISTRATOR);
  }

  if (blocklist_state == URLBlocklistState::URL_IN_ALLOWLIST)
    return PROCEED;

  return CheckSafeSitesFilter(url);
}

// SafeSitesNavigationThrottle is unconditional and does not check PrefService
// because it is used outside //chrome. Therefore, the policy must be checked
// here to determine whether to use SafeSitesNavigationThrottle.
content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::CheckSafeSitesFilter(const GURL& url) {
  SafeSitesFilterBehavior filter_behavior =
      static_cast<SafeSitesFilterBehavior>(
          prefs_->GetInteger(policy::policy_prefs::kSafeSitesFilterBehavior));
  if (filter_behavior == SafeSitesFilterBehavior::kSafeSitesFilterDisabled)
    return PROCEED;

  DCHECK_EQ(filter_behavior, SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  return safe_sites_navigation_throttle_.WillStartRequest();
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

const char* PolicyBlocklistNavigationThrottle::GetNameForLogging() {
  return "PolicyBlocklistNavigationThrottle";
}

void PolicyBlocklistNavigationThrottle::OnFirstPoliciesLoaded(
    policy::PolicyDomain domain) {
  DCHECK(domain == policy::POLICY_DOMAIN_CHROME);
  // Cancel the timeout timer.
  wait_for_policy_timer_.AbandonAndStop();
  OnFirstPoliciesLoadedImpl(/*timeout=*/false);
}

void PolicyBlocklistNavigationThrottle::OnFirstPoliciesLoadedTimeout() {
  OnFirstPoliciesLoadedImpl(/*timeout=*/true);
}

void PolicyBlocklistNavigationThrottle::OnFirstPoliciesLoadedImpl(
    bool timeout) {
  policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  policy_service_ = nullptr;

  const GURL& url = navigation_handle()->GetURL();
  URLBlocklistState blocklist_state =
      blocklist_service_->GetURLBlocklistState(url);
  base::UmaHistogramBoolean(
      "Navigation.PolicyBlocklistNavigationThrottle.PolicyLoadTimeout",
      timeout);
  base::UmaHistogramMediumTimes(
      "Navigation.PolicyBlocklistNavigationThrottle.PolicyLoadDelay",
      base::TimeTicks::Now() - policy_load_throttle_start_time_);
  if (blocklist_state == URLBlocklistState::URL_IN_BLOCKLIST) {
    return CancelDeferredNavigation(
        ThrottleCheckResult(BLOCK_REQUEST, net::ERR_BLOCKED_BY_ADMINISTRATOR));
  }

  if (IsBlockedViewSourceNavigation()) {
    return CancelDeferredNavigation(
        ThrottleCheckResult(BLOCK_REQUEST, net::ERR_BLOCKED_BY_ADMINISTRATOR));
  }

  if (CheckSafeSitesFilter(url).action() == PROCEED)
    Resume();
}

void PolicyBlocklistNavigationThrottle::OnDeferredSafeSitesResult(
    bool is_safe,
    ThrottleCheckResult cancel_result) {
  if (is_safe) {
    Resume();
  } else {
    CancelDeferredNavigation(cancel_result);
  }
}
// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_navigation_throttle.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/policy/content/safe_sites_navigation_throttle.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/policy/core/browser/url_list/url_blocklist_policy_handler.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

using URLBlocklistState = policy::URLBlocklist::URLBlocklistState;
using PolicyBlocklistState = PolicyBlocklistService::PolicyBlocklistState;
using SafeSitesFilterBehavior = policy::SafeSitesFilterBehavior;

// Passing an Unretained pointer for the safe_sites_navigation_throttle_
// callback is safe because this object owns safe_sites_navigation_throttle_,
// which runs the callback from within the object.
PolicyBlocklistNavigationThrottle::PolicyBlocklistNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    PrefService* prefs,
    PolicyBlocklistService* blocklist_service,
    SafeSearchService* safe_search_service)
    : content::NavigationThrottle(registry),
      blocklist_service_(blocklist_service),
      prefs_(prefs) {
  DCHECK(prefs_);
  auto safe_sites_navigation_throttle =
      std::make_unique<SafeSitesNavigationThrottle>(registry,
                                                    safe_search_service);
  if (base::FeatureList::IsEnabled(
          policy::features::kPolicyBlocklistProceedUntilResponse)) {
    safe_sites_navigation_throttle_ =
        std::make_unique<ProceedUntilResponseNavigationThrottle>(
            registry, std::move(safe_sites_navigation_throttle),
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

PolicyBlocklistNavigationThrottle::~PolicyBlocklistNavigationThrottle() =
    default;

bool PolicyBlocklistNavigationThrottle::IsViewSourceNavigation() {
  content::NavigationEntry* nav_entry =
      navigation_handle()->GetNavigationEntry();
  return nav_entry && nav_entry->IsViewSourceMode();
}

PolicyBlocklistState
PolicyBlocklistNavigationThrottle::GetViewSourceNavigationBlocklistState() {
  CHECK(IsViewSourceNavigation());
  GURL view_source_url =
      GURL(std::string("view-source:") + navigation_handle()->GetURL().spec());

  return blocklist_service_->GetURLBlocklistStateWithPolicySource(
      view_source_url);
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillStartOrRedirectRequest(
    bool is_redirect) {
  // Ignore blob scheme because we may use it to deliver navigation responses
  // to the renderer process.
  const GURL& url = navigation_handle()->GetURL();
  if (url.SchemeIs(url::kBlobScheme)) {
    return PROCEED;
  }

  PolicyBlocklistState blocklist_state =
      blocklist_service_->GetURLBlocklistStateWithPolicySource(url);

  if (blocklist_state.url_blocklist_state ==
      URLBlocklistState::URL_IN_BLOCKLIST) {
    return ThrottleCheckResult(
        BLOCK_REQUEST,
        blocklist_state.policy_source == PolicyBlocklistState::INCOGNITO_POLICY
            ? net::ERR_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR
            : net::ERR_BLOCKED_BY_ADMINISTRATOR);
  }

  // If the navigation is to view-source, check if the view-source:url should
  // be blocked.
  if (IsViewSourceNavigation()) {
    PolicyBlocklistState view_source_blocklist_state =
        GetViewSourceNavigationBlocklistState();
    if (view_source_blocklist_state.url_blocklist_state ==
        URLBlocklistState::URL_IN_BLOCKLIST) {
      return ThrottleCheckResult(
          BLOCK_REQUEST, view_source_blocklist_state.policy_source ==
                                 PolicyBlocklistState::INCOGNITO_POLICY
                             ? net::ERR_BLOCKED_IN_INCOGNITO_BY_ADMINISTRATOR
                             : net::ERR_BLOCKED_BY_ADMINISTRATOR);
    }
  }

  if (blocklist_state.url_blocklist_state ==
      URLBlocklistState::URL_IN_ALLOWLIST) {
    return PROCEED;
  }

  return CheckSafeSitesFilter(url, is_redirect);
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
  ThrottleCheckResult result =
      safe_sites_navigation_throttle_->WillProcessResponse();
  return result;
}

const char* PolicyBlocklistNavigationThrottle::GetNameForLogging() {
  return "PolicyBlocklistNavigationThrottle";
}

void PolicyBlocklistNavigationThrottle::OnDeferredSafeSitesResult(
    bool proceed,
    std::optional<ThrottleCheckResult> result) {
  if (proceed) {
    Resume();
  } else {
    CHECK(result.has_value());
    CancelDeferredNavigation(*result);
  }
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blacklist_navigation_throttle.h"

#include "base/bind.h"
#include "base/logging.h"
#include "components/policy/content/policy_blacklist_service.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "components/policy/core/browser/url_blacklist_policy_handler.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

using URLBlacklistState = policy::URLBlacklist::URLBlacklistState;
using SafeSitesFilterBehavior = policy::SafeSitesFilterBehavior;

PolicyBlacklistNavigationThrottle::PolicyBlacklistNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    content::BrowserContext* context)
    : NavigationThrottle(navigation_handle) {
  blacklist_service_ = PolicyBlacklistFactory::GetForBrowserContext(context);
  prefs_ = user_prefs::UserPrefs::Get(context);
  DCHECK(prefs_);
}

PolicyBlacklistNavigationThrottle::~PolicyBlacklistNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlacklistNavigationThrottle::WillStartRequest() {
  GURL url = navigation_handle()->GetURL();

  // Ignore blob scheme because we may use it to deliver navigation responses
  // to the renderer process.
  if (url.SchemeIs(url::kBlobScheme))
    return PROCEED;

  URLBlacklistState blacklist_state =
      blacklist_service_->GetURLBlacklistState(url);
  if (blacklist_state == URLBlacklistState::URL_IN_BLACKLIST) {
    return ThrottleCheckResult(BLOCK_REQUEST,
                               net::ERR_BLOCKED_BY_ADMINISTRATOR);
  }

  if (blacklist_state == URLBlacklistState::URL_IN_WHITELIST)
    return PROCEED;

  // Safe Sites filter applies to top-level HTTP[S] requests.
  if (!url.SchemeIsHTTPOrHTTPS())
    return PROCEED;

  SafeSitesFilterBehavior filter_behavior =
      static_cast<SafeSitesFilterBehavior>(
          prefs_->GetInteger(policy::policy_prefs::kSafeSitesFilterBehavior));
  if (filter_behavior == SafeSitesFilterBehavior::kSafeSitesFilterDisabled)
    return PROCEED;

  DCHECK_EQ(filter_behavior, SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  GURL effective_url = policy::url_util::GetEmbeddedURL(url);
  if (!effective_url.is_valid())
    effective_url = url;

  bool synchronous = blacklist_service_->CheckSafeSearchURL(
      effective_url,
      base::BindOnce(
          &PolicyBlacklistNavigationThrottle::CheckSafeSearchCallback,
          weak_ptr_factory_.GetWeakPtr()));
  if (!synchronous) {
    deferred_ = true;
    return DEFER;
  }

  if (should_cancel_)
    return ThrottleCheckResult(CANCEL, net::ERR_BLOCKED_BY_ADMINISTRATOR);
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlacklistNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

const char* PolicyBlacklistNavigationThrottle::GetNameForLogging() {
  return "PolicyBlacklistNavigationThrottle";
}

void PolicyBlacklistNavigationThrottle::CheckSafeSearchCallback(bool is_safe) {
  if (!deferred_) {
    should_cancel_ = !is_safe;
    return;
  }

  deferred_ = false;
  if (is_safe) {
    Resume();
  } else {
    CancelDeferredNavigation(
        ThrottleCheckResult(CANCEL, net::ERR_BLOCKED_BY_ADMINISTRATOR));
  }
}

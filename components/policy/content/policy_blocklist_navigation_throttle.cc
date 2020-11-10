// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_navigation_throttle.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

using URLBlocklistState = policy::URLBlocklist::URLBlocklistState;
using SafeSitesFilterBehavior = policy::SafeSitesFilterBehavior;

PolicyBlocklistNavigationThrottle::PolicyBlocklistNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    content::BrowserContext* context)
    : NavigationThrottle(navigation_handle),
      blocklist_service_(PolicyBlocklistFactory::GetForBrowserContext(context)),
      prefs_(user_prefs::UserPrefs::Get(context)) {
  DCHECK(prefs_);
}

PolicyBlocklistNavigationThrottle::PolicyBlocklistNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    content::BrowserContext* context,
    base::StringPiece safe_sites_error_page_content)
    : PolicyBlocklistNavigationThrottle(navigation_handle, context) {
  safe_sites_error_page_content_ = safe_sites_error_page_content.as_string();
}

PolicyBlocklistNavigationThrottle::~PolicyBlocklistNavigationThrottle() =
    default;

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillStartRequest() {
  GURL url = navigation_handle()->GetURL();

  // Ignore blob scheme because we may use it to deliver navigation responses
  // to the renderer process.
  if (url.SchemeIs(url::kBlobScheme))
    return PROCEED;

  URLBlocklistState blocklist_state =
      blocklist_service_->GetURLBlocklistState(url);
  if (blocklist_state == URLBlocklistState::URL_IN_BLOCKLIST) {
    return ThrottleCheckResult(BLOCK_REQUEST,
                               net::ERR_BLOCKED_BY_ADMINISTRATOR);
  }

  if (blocklist_state == URLBlocklistState::URL_IN_ALLOWLIST)
    return PROCEED;

  return CheckSafeSitesFilter(url);
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::CheckSafeSitesFilter(const GURL& url) {
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

  bool synchronous = blocklist_service_->CheckSafeSearchURL(
      effective_url,
      base::BindOnce(
          &PolicyBlocklistNavigationThrottle::CheckSafeSearchCallback,
          weak_ptr_factory_.GetWeakPtr()));
  if (!synchronous) {
    deferred_ = true;
    return DEFER;
  }

  if (should_cancel_)
    return ThrottleCheckResult(CANCEL, net::ERR_BLOCKED_BY_ADMINISTRATOR,
                               safe_sites_error_page_content_);
  return PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
PolicyBlocklistNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

const char* PolicyBlocklistNavigationThrottle::GetNameForLogging() {
  return "PolicyBlocklistNavigationThrottle";
}

void PolicyBlocklistNavigationThrottle::CheckSafeSearchCallback(bool is_safe) {
  if (!deferred_) {
    should_cancel_ = !is_safe;
    return;
  }

  deferred_ = false;
  if (is_safe) {
    Resume();
  } else {
    CancelDeferredNavigation(
        ThrottleCheckResult(CANCEL, net::ERR_BLOCKED_BY_ADMINISTRATOR,
                            safe_sites_error_page_content_));
  }
}

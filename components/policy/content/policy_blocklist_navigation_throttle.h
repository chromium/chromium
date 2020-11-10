// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_NAVIGATION_THROTTLE_H_
#define COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_NAVIGATION_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;
class PolicyBlocklistService;
class PrefService;

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

// PolicyBlocklistNavigationThrottle provides a simple way to block a navigation
// based on the URLBlocklistManager and Safe Search API. If the URL is on the
// blocklist or allowlist, the throttle will immediately block or allow the
// navigation. Otherwise, the URL will be checked against the Safe Search API if
// the SafeSitesFilterBehavior policy is enabled. This final check may be
// asynchronous if the result hasn't been cached yet.
class PolicyBlocklistNavigationThrottle : public content::NavigationThrottle {
 public:
  PolicyBlocklistNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      content::BrowserContext* context);
  PolicyBlocklistNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      content::BrowserContext* context,
      base::StringPiece safe_sites_error_page_content);
  ~PolicyBlocklistNavigationThrottle() override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult CheckSafeSitesFilter(const GURL& url);

  // Callback from PolicyBlocklistService.
  void CheckSafeSearchCallback(bool is_safe);

  PolicyBlocklistService* blocklist_service_;

  PrefService* prefs_;

  // HTML to be displayed when navigation is canceled by the Safe Sites filter.
  // If null, a default error page will be displayed.
  base::Optional<std::string> safe_sites_error_page_content_;

  // Whether the request was deferred in order to check the Safe Search API.
  bool deferred_ = false;

  // Whether the Safe Search API callback determined the in-progress navigation
  // should be canceled.
  bool should_cancel_ = false;

  base::WeakPtrFactory<PolicyBlocklistNavigationThrottle> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PolicyBlocklistNavigationThrottle);
};

#endif  // COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_NAVIGATION_THROTTLE_H_

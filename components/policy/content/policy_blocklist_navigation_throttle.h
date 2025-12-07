// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_NAVIGATION_THROTTLE_H_
#define COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_NAVIGATION_THROTTLE_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;
class PolicyBlocklistService;
class PrefService;
class SafeSearchService;

namespace content {
class NavigationThrottleRegistry;
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
      content::NavigationThrottleRegistry& registry,
      PrefService* prefs,
      PolicyBlocklistService* blocklist_service,
      SafeSearchService* safe_search_service);
  PolicyBlocklistNavigationThrottle(const PolicyBlocklistNavigationThrottle&) =
      delete;
  PolicyBlocklistNavigationThrottle& operator=(
      const PolicyBlocklistNavigationThrottle&) = delete;
  ~PolicyBlocklistNavigationThrottle() override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PolicyBlocklistNavigationThrottleTest, Blocklist);
  FRIEND_TEST_ALL_PREFIXES(PolicyBlocklistNavigationThrottleTest, Allowlist);
  FRIEND_TEST_ALL_PREFIXES(PolicyBlocklistNavigationThrottleTest,
                           SafeSites_Safe);
  FRIEND_TEST_ALL_PREFIXES(PolicyBlocklistNavigationThrottleTest,
                           SafeSites_Porn);

  // Returns TRUE if this navigation is to view-source: and view-source is on
  // the URLBlocklist.
  bool IsBlockedViewSourceNavigation();

  // To ensure both allow and block policies override Safe Sites,
  // SafeSitesNavigationThrottle must be consulted as part of this throttle
  // rather than added separately to the list of throttles.
  ThrottleCheckResult CheckSafeSitesFilter(const GURL& url, bool is_redirect);
  void OnDeferredSafeSitesResult(bool proceed,
                                 std::optional<ThrottleCheckResult> result);

  ThrottleCheckResult WillStartOrRedirectRequest(bool is_redirect);

  std::unique_ptr<content::NavigationThrottle> safe_sites_navigation_throttle_;

  const raw_ptr<PolicyBlocklistService, DanglingUntriaged> blocklist_service_;

  const raw_ptr<PrefService> prefs_;
};

#endif  // COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_NAVIGATION_THROTTLE_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_POLICY_BLACKLIST_NAVIGATION_THROTTLE_H_
#define COMPONENTS_POLICY_CONTENT_POLICY_BLACKLIST_NAVIGATION_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

class PolicyBlacklistService;
class PrefService;

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

// PolicyBlacklistNavigationThrottle provides a simple way to block a navigation
// based on the URLBlacklistManager and Safe Search API. If the URL is
// blacklisted or whitelisted, the throttle will immediately block or allow the
// navigation. Otherwise, the URL will be checked against the Safe Search API if
// the SafeSitesFilterBehavior policy is enabled. This final check may be
// asynchronous if the result hasn't been cached yet.
class PolicyBlacklistNavigationThrottle : public content::NavigationThrottle {
 public:
  PolicyBlacklistNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      content::BrowserContext* context);
  ~PolicyBlacklistNavigationThrottle() override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

  const char* GetNameForLogging() override;

 private:
  // Callback from PolicyBlacklistService.
  void CheckSafeSearchCallback(bool is_safe);

  PolicyBlacklistService* blacklist_service_;

  PrefService* prefs_;

  // Whether the request was deferred in order to check the Safe Search API.
  bool deferred_ = false;

  // Whether the Safe Search API callback determined the in-progress navigation
  // should be canceled.
  bool should_cancel_ = false;

  base::WeakPtrFactory<PolicyBlacklistNavigationThrottle> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(PolicyBlacklistNavigationThrottle);
};

#endif  // COMPONENTS_POLICY_CONTENT_POLICY_BLACKLIST_NAVIGATION_THROTTLE_H_

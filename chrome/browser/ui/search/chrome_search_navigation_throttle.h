// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_CHROME_SEARCH_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_SEARCH_CHROME_SEARCH_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

// NavigationThrottle that blocks chrome-search: navigations from non-instant
// processes. This enforces that only instant processes can commit chrome-search
// URLs, as registered via RegisterWebSafeIsolatedScheme.
//
// ChromeSearchNavigationThrottle is only registered when the navigation's
// initial URL uses the chrome-search scheme (see MaybeCreateAndAdd). Redirects
// to chrome-search are not handled in this throttle; they are already blocked
// by ChildProcessSecurityPolicy::CanCommitOriginAndUrl at commit time.
class ChromeSearchNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit ChromeSearchNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ~ChromeSearchNavigationThrottle() override;

  ChromeSearchNavigationThrottle(const ChromeSearchNavigationThrottle&) =
      delete;
  ChromeSearchNavigationThrottle& operator=(
      const ChromeSearchNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;

  // Creates and adds the throttle only when the navigation is
  // renderer-initiated to chrome-search: and the profile has InstantService.
  // This avoids registering for every navigation.
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);
};

#endif  // CHROME_BROWSER_UI_SEARCH_CHROME_SEARCH_NAVIGATION_THROTTLE_H_

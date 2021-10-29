// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace safe_browsing {

class SafeBrowsingUIManager;

// This throttle monitors failed requests, and if a request failed due to it
// being blocked by Safe Browsing, it creates and displays an interstitial.
// This throttle is only created when Safe Browsing committed interstitials are
// enabled.
class SafeBrowsingNavigationThrottle : public content::NavigationThrottle {
 public:
  // |ui_manager| may be null, in which case no interstitials are created.
  SafeBrowsingNavigationThrottle(content::NavigationHandle* handle,
                                 SafeBrowsingUIManager* ui_manager);
  ~SafeBrowsingNavigationThrottle() override = default;
  const char* GetNameForLogging() override;

  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;

 private:
  SafeBrowsingUIManager* manager_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_THROTTLE_H_

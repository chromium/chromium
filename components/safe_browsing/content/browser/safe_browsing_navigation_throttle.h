// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace safe_browsing {

class SafeBrowsingUIManager;

// This throttle monitors failed requests in an outer-most main frame (e.g.
// doesn't apply for fenced-frames), and if a request failed due to
// it being blocked by Safe Browsing, it creates and displays an interstitial.
// For other kinds of loads, the interstitial is navigated at the same time the
// load is canceled in BaseUIManager::DisplayBlockingPage
//
// This NavigationThrottle doesn't actually perform a SafeBrowsing check nor
// block the navigation.  That happens in SafeBrowsing's
// BrowserURLLoaderThrottle and RendererURLLoaderThrottles and related code.
// Those cause the navigation to fail which invokes this throttle to show the
// interstitial.
class SafeBrowsingNavigationThrottle : public content::NavigationThrottle {
 public:
  // |ui_manager| may be null, in which case no throttle is created.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle,
      SafeBrowsingUIManager* ui_manager);
  ~SafeBrowsingNavigationThrottle() override = default;
  const char* GetNameForLogging() override;

  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;

 private:
  SafeBrowsingNavigationThrottle(content::NavigationHandle* handle,
                                 SafeBrowsingUIManager* ui_manager);

  raw_ptr<SafeBrowsingUIManager> manager_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_THROTTLE_H_

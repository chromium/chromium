// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TABBED_WEB_APP_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TABBED_WEB_APP_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace web_app {

// A navigation throttle that helps tabbed web apps with a pinned home tab
// to open links in the correct tab.
class TabbedWebAppNavigationThrottle : public content::NavigationThrottle {
 public:
  // Returns a navigation throttle when the navigation is happening inside
  // a tabbed web app and the tabbed web app has a pinned home tab.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  explicit TabbedWebAppNavigationThrottle(content::NavigationHandle* handle);
  ~TabbedWebAppNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  // Links clicked from the home tab should open in a new app tab.
  ThrottleCheckResult OpenInNewTab();
  // Navigations to the home tab URL should open in the home tab.
  ThrottleCheckResult FocusHomeTab();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TABBED_WEB_APP_NAVIGATION_THROTTLE_H_

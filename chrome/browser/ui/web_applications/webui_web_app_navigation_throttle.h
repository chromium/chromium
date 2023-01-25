// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEBUI_WEB_APP_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEBUI_WEB_APP_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace web_app {

// A navigation throttle that helps WebUI web apps to open links in the
// correct tab.
class WebUIWebAppNavigationThrottle : public content::NavigationThrottle {
 public:
  // Returns a navigation throttle when the navigation is happening inside
  // a web app with a WebUI start url.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  explicit WebUIWebAppNavigationThrottle(content::NavigationHandle* handle);
  ~WebUIWebAppNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEBUI_WEB_APP_NAVIGATION_THROTTLE_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_CAPTURING_REDIRECTION_THROTTLE_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_CAPTURING_REDIRECTION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

// Navigation throttle used to handle navigation capturing at the end of a
// redirect chain.
class NavigationCapturingRedirectionThrottle
    : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  NavigationCapturingRedirectionThrottle(
      const NavigationCapturingRedirectionThrottle&) = delete;
  NavigationCapturingRedirectionThrottle& operator=(
      const NavigationCapturingRedirectionThrottle&) = delete;
  ~NavigationCapturingRedirectionThrottle() override;

  // content::NavigationHandle overrides:
  const char* GetNameForLogging() override;

  // This is where the data stored via the
  // `NavigationCapturingNavigationHandleUserData` is processed.
  ThrottleCheckResult WillProcessResponse() override;

 private:
  explicit NavigationCapturingRedirectionThrottle(
      content::NavigationHandle* navigation_handle);

  ThrottleCheckResult HandleRequest();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_CAPTURING_REDIRECTION_THROTTLE_H_

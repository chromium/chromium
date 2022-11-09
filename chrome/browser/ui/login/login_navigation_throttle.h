// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

// LoginNavigationThrottle intercepts navigations that serve auth challenges and
// forwards them to LoginTabHelper to rewrite the response to a blank
// page. LoginTabHelper displays the login prompt on top of this blank page.
//
// Relevant navigations are forwarded to LoginTabHelper, rather than rewritten
// directly in LoginNavigationThrottle, because LoginTabHelper tracks navigation
// state to handle login prompt cancellations.
class LoginNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit LoginNavigationThrottle(content::NavigationHandle* handle);
  ~LoginNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_NAVIGATION_THROTTLE_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_MANAGER_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_MANAGER_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

// This NavigationThrottle redirects to Passwords in Settings when a target
// and referrer matches defined requirements.
class PasswordManagerNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit PasswordManagerNavigationThrottle(content::NavigationHandle* handle);

  ~PasswordManagerNavigationThrottle() override;

  static std::unique_ptr<PasswordManagerNavigationThrottle>
  MaybeCreateThrottleFor(content::NavigationHandle* handle);

 private:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_MANAGER_NAVIGATION_THROTTLE_H_

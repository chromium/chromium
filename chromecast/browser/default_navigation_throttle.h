// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEFAULT_NAVIGATION_THROTTLE_H_
#define CHROMECAST_BROWSER_DEFAULT_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

class DefaultNavigationThrottle : public content::NavigationThrottle {
 public:
  DefaultNavigationThrottle(content::NavigationHandle* handle,
                            NavigationThrottle::ThrottleAction default_action);
  ~DefaultNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 private:
  content::NavigationThrottle::ThrottleAction const default_action_;
};

#endif  // CHROMECAST_BROWSER_DEFAULT_NAVIGATION_THROTTLE_H_

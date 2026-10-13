// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_AD_SCRIPT_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_AD_SCRIPT_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}

// A throttle that records unwanted navigations triggered from ad scripts.
// TODO(jkarlin): Intervene on these in the future and use popup-blocker UX.
class AdScriptNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit AdScriptNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  AdScriptNavigationThrottle(const AdScriptNavigationThrottle&) = delete;
  AdScriptNavigationThrottle& operator=(const AdScriptNavigationThrottle&) =
      delete;
  ~AdScriptNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_AD_SCRIPT_NAVIGATION_THROTTLE_H_

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_NEW_TAB_PAGE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_SEARCH_NEW_TAB_PAGE_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

// A NavigationThrottle that opens the local New Tab Page when there is any
// issue opening the remote New Tab Page.
class NewTabPageNavigationThrottle : public content::NavigationThrottle {
 public:
  // Returns a NavigationThrottle when:
  // - we are navigating to the new tab page, and
  // - the main frame is pointed at the new tab URL.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  explicit NewTabPageNavigationThrottle(content::NavigationHandle* handle);
  ~NewTabPageNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult OpenLocalNewTabPage();
};

#endif  // CHROME_BROWSER_UI_SEARCH_NEW_TAB_PAGE_NAVIGATION_THROTTLE_H_

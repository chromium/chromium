// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_NAVIGATION_THROTTLE_H_

#include "chrome/browser/themes/theme_service.h"
#include "content/public/browser/navigation_throttle.h"

namespace lens {
// This class prevents users from opening cross-origin links in the lens overlay
// results side panel. It also prevents invalid search results URLs from loading
// in the frame and instead cancels the navigation to start a new navigation
// with the corrected search URL.
class LensOverlaySidePanelNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // Static function that creates the navigation throttle for the provided
  // handle if eligible.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateFor(
      content::NavigationHandle* handle,
      ThemeService* theme_service);

  // NavigationThrottle overrides:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

  const char* GetNameForLogging() override;

 private:
  explicit LensOverlaySidePanelNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      ThemeService* theme_service);

  ThrottleCheckResult HandleSidePanelRequest();

  // The theme service associated with the current profile.
  raw_ptr<ThemeService> theme_service_;
};
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_NAVIGATION_THROTTLE_H_

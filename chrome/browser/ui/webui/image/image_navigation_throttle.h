// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IMAGE_IMAGE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_WEBUI_IMAGE_IMAGE_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

// Blocks navigations to chrome://image URLs.
class ImageNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  explicit ImageNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ~ImageNavigationThrottle() override;
  ImageNavigationThrottle(const ImageNavigationThrottle&) = delete;
  ImageNavigationThrottle& operator=(const ImageNavigationThrottle&) = delete;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_IMAGE_IMAGE_NAVIGATION_THROTTLE_H_

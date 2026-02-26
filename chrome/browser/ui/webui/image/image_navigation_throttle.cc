// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/image/image_navigation_throttle.h"

#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

// static
void ImageNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.GetURL().SchemeIs(content::kChromeUIScheme) ||
      handle.GetURL().host() != chrome::kChromeUIImageHost) {
    return;
  }
  registry.AddThrottle(std::make_unique<ImageNavigationThrottle>(registry));
}

ImageNavigationThrottle::ImageNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

ImageNavigationThrottle::~ImageNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
ImageNavigationThrottle::WillStartRequest() {
  return content::NavigationThrottle::BLOCK_REQUEST;
}

const char* ImageNavigationThrottle::GetNameForLogging() {
  return "ImageNavigationThrottle";
}

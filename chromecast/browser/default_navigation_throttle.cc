// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/default_navigation_throttle.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"

DefaultNavigationThrottle::DefaultNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    content::NavigationThrottle::ThrottleAction default_action)
    : content::NavigationThrottle(navigation_handle),
      default_action_(default_action) {}

DefaultNavigationThrottle::~DefaultNavigationThrottle() {}

const char* DefaultNavigationThrottle::GetNameForLogging() {
  return "DefaultNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
DefaultNavigationThrottle::WillStartRequest() {
  // Perform the default action for all requests except the first.
  if (navigation_handle()->HasUserGesture() &&
      (navigation_handle()->GetStartingSiteInstance()->GetSiteURL() !=
       navigation_handle()->GetURL())) {
    return default_action_;
  }
  return content::NavigationThrottle::PROCEED;
}

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/password_protection/password_protection_navigation_throttle.h"

#include "components/safe_browsing/content/password_protection/password_protection_request.h"
#include "content/public/browser/navigation_handle.h"

namespace safe_browsing {
PasswordProtectionNavigationThrottle::PasswordProtectionNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    scoped_refptr<PasswordProtectionRequest> request,
    bool is_warning_showing)
    : content::NavigationThrottle(navigation_handle),
      request_(request),
      is_warning_showing_(is_warning_showing) {
  // Only call AddThrottle() if there is no modal warning showing. If there's a
  // modal dialog, PPNavigationThrottle will simply cancel this navigation
  // immediately, therefore no need to keep track of it.
  if (!is_warning_showing_)
    request_->AddThrottle(this);
}

PasswordProtectionNavigationThrottle::~PasswordProtectionNavigationThrottle() {
  if (request_)
    request_->RemoveThrottle(this);
}

content::NavigationThrottle::ThrottleCheckResult
PasswordProtectionNavigationThrottle::WillStartRequest() {
  // If a modal warning is being shown right now, we don't
  // want to continue navigation. Otherwise, we assume that
  // the PasswordProtectionRequest is still waiting for a
  // verdict and so we defer the navigation.
  if (is_warning_showing_)
    return content::NavigationThrottle::CANCEL;
  return content::NavigationThrottle::DEFER;
}

content::NavigationThrottle::ThrottleCheckResult
PasswordProtectionNavigationThrottle::WillRedirectRequest() {
  // If a modal warning is being shown right now, we don't
  // want to redirect navigation. Otherwise, if the
  // PasswordProtectionRequest still exists, we assume that the
  // request is still waiting for a verdict and so we defer the
  // navigation, otherwise we proceed navigation.
  if (is_warning_showing_)
    return content::NavigationThrottle::CANCEL;
  return request_ ? content::NavigationThrottle::DEFER
                  : content::NavigationThrottle::PROCEED;
}

const char* PasswordProtectionNavigationThrottle::GetNameForLogging() {
  return "PasswordProtectionNavigationThrottle";
}

void PasswordProtectionNavigationThrottle::ResumeNavigation() {
  Resume();
  // When navigation is resumed, we do not need to keep track of the
  // PasswordProtectionRequest because this method is only called
  // after the request received a verdict and has finished.
  request_.reset();
}

void PasswordProtectionNavigationThrottle::CancelNavigation(
    content::NavigationThrottle::ThrottleCheckResult result) {
  // When navigation is resumed, we do not need to keep track of the
  // PasswordProtectionRequest because this method is only called
  // after the request received a verdict, showing a modal warning and has
  // finished.
  CancelDeferredNavigation(result);
  request_.reset();
}

}  // namespace safe_browsing

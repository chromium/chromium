// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_PASSWORD_PROTECTION_PASSWORD_PROTECTION_NAVIGATION_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_PASSWORD_PROTECTION_PASSWORD_PROTECTION_NAVIGATION_THROTTLE_H_

#include "base/memory/ref_counted.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace safe_browsing {
class PasswordProtectionRequest;

// PasswordProtectionNavigationThrottle defers or cancel navigation under the
// following condition:
// (1) if a navigation starts when there is a on-going sync password reuse ping,
//     this throttle defers this navigation. When the verdict comes back, if the
//     verdict results in showing a modal warning dialog, the deferred
//     navigation will be canceled; otherwise, the deferred navigation will be
//     resumed.
// (2) if a navigation starts when there is a modal warning showing, this
//     throttle simply cancels this navigation.
class PasswordProtectionNavigationThrottle
    : public content::NavigationThrottle {
 public:
  PasswordProtectionNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      scoped_refptr<PasswordProtectionRequest> request,
      bool is_warning_showing);
  ~PasswordProtectionNavigationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  const char* GetNameForLogging() override;

  // Called to resume a deferred navigation once the PasswordProtectionRequest
  // has received a verdict and there is no modal warning shown.
  void ResumeNavigation();
  // Called when the PasswordProtectionRequest has received a verdict and there
  // is a modal warning shown.
  void CancelNavigation(
      content::NavigationThrottle::ThrottleCheckResult result);

 private:
  scoped_refptr<PasswordProtectionRequest> request_;
  bool is_warning_showing_;
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionNavigationThrottle);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_PASSWORD_PROTECTION_PASSWORD_PROTECTION_NAVIGATION_THROTTLE_H_

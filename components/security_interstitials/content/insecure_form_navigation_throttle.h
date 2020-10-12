// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_NAVIGATION_THROTTLE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_NAVIGATION_THROTTLE_H_

#include "components/security_interstitials/content/security_blocking_page_factory.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

class PrefService;

namespace security_interstitials {

class InsecureFormNavigationThrottle : public content::NavigationThrottle {
 public:
  InsecureFormNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory);
  ~InsecureFormNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  static std::unique_ptr<InsecureFormNavigationThrottle>
  MaybeCreateNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
      PrefService* prefs);

 private:
  std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory_;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_INSECURE_FORM_NAVIGATION_THROTTLE_H_

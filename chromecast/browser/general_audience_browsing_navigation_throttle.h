// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_GENERAL_AUDIENCE_BROWSING_NAVIGATION_THROTTLE_H_
#define CHROMECAST_BROWSER_GENERAL_AUDIENCE_BROWSING_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace chromecast {

class GeneralAudienceBrowsingService;

class GeneralAudienceBrowsingNavigationThrottle
    : public content::NavigationThrottle {
 public:
  GeneralAudienceBrowsingNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      GeneralAudienceBrowsingService* general_audience_browsing_service);

  GeneralAudienceBrowsingNavigationThrottle(
      const GeneralAudienceBrowsingNavigationThrottle&) = delete;
  GeneralAudienceBrowsingNavigationThrottle& operator=(
      const GeneralAudienceBrowsingNavigationThrottle&) = delete;

  ~GeneralAudienceBrowsingNavigationThrottle() override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

  const char* GetNameForLogging() override;

 private:
  content::NavigationThrottle::ThrottleCheckResult CheckURL();

  // Callback from GeneralAudienceBrowsingService.
  void CheckURLCallback(bool is_safe);

  GeneralAudienceBrowsingService* general_audience_browsing_service_;

  // Whether the request was deferred in order to check the Safe Search API.
  bool deferred_ = false;

  // Whether the Safe Search API callback determined the in-progress navigation
  // should be canceled.
  bool should_cancel_ = false;

  base::WeakPtrFactory<GeneralAudienceBrowsingNavigationThrottle>
      weak_ptr_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_GENERAL_AUDIENCE_BROWSING_NAVIGATION_THROTTLE_H_

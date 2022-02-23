// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_METRIC_THROTTLE_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_METRIC_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace url_param_filter {

// This class is used to log HTTP response codes of links opened from non-OTR
// to OTR browsing. It is required to monitor potential breakage from a URL
// parameter filtering experiment.
class CrossOtrMetricNavigationThrottle : public content::NavigationThrottle {
 public:
  // Creates a throttle if the characteristics of the navigation match a switch
  // to Off The Record from normal browsing.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  CrossOtrMetricNavigationThrottle(const CrossOtrMetricNavigationThrottle&) =
      delete;
  CrossOtrMetricNavigationThrottle& operator=(
      const CrossOtrMetricNavigationThrottle&) = delete;

  ~CrossOtrMetricNavigationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

 private:
  explicit CrossOtrMetricNavigationThrottle(content::NavigationHandle* handle);
};

}  // namespace url_param_filter

#endif  // CHROME_BROWSER_URL_PARAM_FILTER_CROSS_OTR_METRIC_THROTTLE_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_NAVIGATION_THROTTLE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"

namespace page_load_metrics {

// This class is used to forward calls to the MetricsWebContentsObserver.
// Namely, WillStartRequest() is called on NavigationThrottles, but not on
// WebContentsObservers.
class MetricsNavigationThrottle : public content::NavigationThrottle {
 public:
  static void CreateAndAdd(content::NavigationThrottleRegistry& registry);

  MetricsNavigationThrottle(const MetricsNavigationThrottle&) = delete;
  MetricsNavigationThrottle& operator=(const MetricsNavigationThrottle&) =
      delete;

  ~MetricsNavigationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

 private:
  explicit MetricsNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_NAVIGATION_THROTTLE_H_

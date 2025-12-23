// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "content/public/browser/navigation_handle.h"

namespace page_load_metrics {

// static
void MetricsNavigationThrottle::CreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  registry.AddThrottle(
      base::WrapUnique(new MetricsNavigationThrottle(registry)));
}

MetricsNavigationThrottle::~MetricsNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
MetricsNavigationThrottle::WillProcessResponse() {
  MetricsWebContentsObserver* observer =
      MetricsWebContentsObserver::FromWebContents(
          navigation_handle()->GetWebContents());
  if (observer) {
    observer->WillProcessNavigationResponse(navigation_handle());
  }
  return content::NavigationThrottle::PROCEED;
}

const char* MetricsNavigationThrottle::GetNameForLogging() {
  return "MetricsNavigationThrottle";
}

MetricsNavigationThrottle::MetricsNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

}  // namespace page_load_metrics

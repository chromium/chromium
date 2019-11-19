// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_test_content_browser_client.h"

#include <memory>
#include <vector>

#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "content/public/browser/navigation_handle.h"

namespace page_load_metrics {

PageLoadMetricsTestContentBrowserClient::
    PageLoadMetricsTestContentBrowserClient() = default;

PageLoadMetricsTestContentBrowserClient::
    ~PageLoadMetricsTestContentBrowserClient() = default;

std::vector<std::unique_ptr<content::NavigationThrottle>>
PageLoadMetricsTestContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
  if (navigation_handle->IsInMainFrame()) {
    throttles.push_back(page_load_metrics::MetricsNavigationThrottle::Create(
        navigation_handle));
  }
  return throttles;
}

}  // namespace page_load_metrics

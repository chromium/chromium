// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_TEST_CONTENT_BROWSER_CLIENT_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_TEST_CONTENT_BROWSER_CLIENT_H_

#include "content/public/browser/content_browser_client.h"

namespace page_load_metrics {

// The content::ContentBrowserClient should be used for page load metrics tests
// in components.
class PageLoadMetricsTestContentBrowserClient
    : public content::ContentBrowserClient {
 public:
  PageLoadMetricsTestContentBrowserClient();

  PageLoadMetricsTestContentBrowserClient(
      const PageLoadMetricsTestContentBrowserClient&) = delete;
  PageLoadMetricsTestContentBrowserClient& operator=(
      const PageLoadMetricsTestContentBrowserClient&) = delete;

  ~PageLoadMetricsTestContentBrowserClient() override;

  // content::ContentBrowserClient:
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_TEST_CONTENT_BROWSER_CLIENT_H_

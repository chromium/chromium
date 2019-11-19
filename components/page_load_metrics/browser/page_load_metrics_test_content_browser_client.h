// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_TEST_CONTENT_BROWSER_CLIENT_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_TEST_CONTENT_BROWSER_CLIENT_H_

#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"

namespace page_load_metrics {

// The content::ContentBrowserClient should be used for page load metrics tests
// in components.
class PageLoadMetricsTestContentBrowserClient
    : public content::ContentBrowserClient {
 public:
  PageLoadMetricsTestContentBrowserClient();
  ~PageLoadMetricsTestContentBrowserClient() override;

  // content::ContentBrowserClient:
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsTestContentBrowserClient);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_TEST_CONTENT_BROWSER_CLIENT_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_ADS_PAGE_LOAD_METRICS_TEST_WAITER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_ADS_PAGE_LOAD_METRICS_TEST_WAITER_H_

#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"

namespace page_load_metrics {

// A PageLoadMetricsTestWaiter that is customized to wait for ads resources.
class AdsPageLoadMetricsTestWaiter
    : public page_load_metrics::PageLoadMetricsTestWaiter {
 public:
  explicit AdsPageLoadMetricsTestWaiter(content::WebContents* web_contents);

  // Sets the minimum number of ads resources to wait for.
  void AddMinimumAdResourceExpectation(int num_ad_resources);

 protected:
  // PageLoadMetricsTestWaiter:
  bool ExpectationsSatisfied() const override;
  void HandleResourceUpdate(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) override;

 private:
  int complete_ad_resources_ = 0;
  int expected_minimum_ad_resources_ = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_ADS_PAGE_LOAD_METRICS_TEST_WAITER_H_

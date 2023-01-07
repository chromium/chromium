// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/ads_page_load_metrics_test_waiter.h"

namespace page_load_metrics {

AdsPageLoadMetricsTestWaiter::AdsPageLoadMetricsTestWaiter(
    content::WebContents* web_contents)
    : page_load_metrics::PageLoadMetricsTestWaiter(web_contents) {}

void AdsPageLoadMetricsTestWaiter::AddMinimumAdResourceExpectation(
    int num_ad_resources) {
  expected_minimum_ad_resources_ = num_ad_resources;
}

bool AdsPageLoadMetricsTestWaiter::ExpectationsSatisfied() const {
  return complete_ad_resources_ >= expected_minimum_ad_resources_ &&
         PageLoadMetricsTestWaiter::ExpectationsSatisfied();
}

void AdsPageLoadMetricsTestWaiter::HandleResourceUpdate(
    const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
  if (resource->reported_as_ad_resource && resource->is_complete)
    complete_ad_resources_++;
}

}  // namespace page_load_metrics

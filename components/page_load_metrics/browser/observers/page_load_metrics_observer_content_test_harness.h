// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_CONTENT_TEST_HARNESS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_CONTENT_TEST_HARNESS_H_

#include <memory>

#include "base/macros.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_content_browser_client.h"
#include "content/public/test/test_renderer_host.h"

namespace page_load_metrics {

class PageLoadTracker;

// This class can be used to drive tests of PageLoadMetricsObservers in
// components. To hook up an observer, override RegisterObservers and call
// tracker->AddObserver. This will attach the observer to all main frame
// navigations.
//
// Refer to PageLoadMetricsObserverTesterInterface for the methods the can be
// used in test.
class PageLoadMetricsObserverContentTestHarness
    : public content::RenderViewHostTestHarness {
 public:
  PageLoadMetricsObserverContentTestHarness();
  ~PageLoadMetricsObserverContentTestHarness() override;

  void SetUp() override;
  void TearDown() override;

  virtual void RegisterObservers(PageLoadTracker* tracker) {}

  PageLoadMetricsObserverTester* tester() { return tester_.get(); }
  const PageLoadMetricsObserverTester* tester() const { return tester_.get(); }

 private:
  std::unique_ptr<PageLoadMetricsObserverTester> tester_;
  PageLoadMetricsTestContentBrowserClient browser_client_;
  content::ContentBrowserClient* original_browser_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsObserverContentTestHarness);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_CONTENT_TEST_HARNESS_H_

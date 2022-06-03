// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_LAYOUT_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_LAYOUT_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace page_load_metrics {

// An observer to record layout-related data.
class LayoutPageLoadMetricsObserver : public PageLoadMetricsObserver {
 public:
  LayoutPageLoadMetricsObserver();
  ~LayoutPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver overrides.
  void OnComplete(const mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const mojom::PageLoadTiming& timing) override;

  LayoutPageLoadMetricsObserver(const LayoutPageLoadMetricsObserver&) = delete;
  LayoutPageLoadMetricsObserver& operator=(
      const LayoutPageLoadMetricsObserver&) = delete;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_LAYOUT_PAGE_LOAD_METRICS_OBSERVER_H_

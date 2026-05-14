// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_DECLARATIVE_PERFORMANCE_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_DECLARATIVE_PERFORMANCE_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace page_load_metrics {

class DeclarativePerformanceObserver : public PageLoadMetricsObserver {
 public:
  DeclarativePerformanceObserver();
  ~DeclarativePerformanceObserver() override;

  DeclarativePerformanceObserver(const DeclarativePerformanceObserver&) =
      delete;
  DeclarativePerformanceObserver& operator=(
      const DeclarativePerformanceObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_DECLARATIVE_PERFORMANCE_OBSERVER_H_

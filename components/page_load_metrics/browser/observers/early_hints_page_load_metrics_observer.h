// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_EARLY_HINTS_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_EARLY_HINTS_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramEarlyHintsPreloadFirstContentfulPaint[];
extern const char kHistogramEarlyHintsPreloadLargestContentfulPaint[];
extern const char kHistogramEarlyHintsPreloadFirstInputDelay[];

}  // namespace internal

// Records custom page load timing metrics for pages that received preload Link
// headers via Early Hints responses.
class EarlyHintsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  EarlyHintsPageLoadMetricsObserver();
  ~EarlyHintsPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordHistograms(const page_load_metrics::mojom::PageLoadTiming& timing);
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_EARLY_HINTS_PAGE_LOAD_METRICS_OBSERVER_H_

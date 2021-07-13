// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramPrerenderNavigationToActivation[];
extern const char kHistogramPrerenderActivationToFirstPaint[];
extern const char kHistogramPrerenderActivationToFirstContentfulPaint[];
extern const char kHistogramPrerenderActivationToLargestContentfulPaint2[];
extern const char kHistogramPrerenderFirstInputDelay4[];
extern const char kHistogramPrerenderCumulativeShiftScore[];
extern const char kHistogramPrerenderCumulativeShiftScoreMainFrame[];

}  // namespace internal

// Prerender2 (content/browser/prerender/README.md):
// Records custom page load timing metrics for prerendered page loads.
class PrerenderPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PrerenderPageLoadMetricsObserver() = default;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordSessionEndHistograms(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_

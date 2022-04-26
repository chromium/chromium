// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  PrerenderPageLoadMetricsObserver();
  ~PrerenderPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
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

  // Helper function to concatenate the histogram name, the trigger type and the
  // embedder histogram suffix when the trigger type is kEmbedder.
  std::string AppendSuffix(const std::string& histogram_name) const;

  // The type to trigger prerendering.
  absl::optional<content::PrerenderTriggerType> trigger_type_;
  // The suffix of a prerender embedder. This value is valid only when
  // PrerenderTriggerType is kEmbedder. Otherwise, it's an empty string.
  std::string embedder_histogram_suffix_;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_

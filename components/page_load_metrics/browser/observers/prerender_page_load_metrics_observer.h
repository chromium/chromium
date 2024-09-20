// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_

#include <optional>

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/preloading_trigger_type.h"

namespace internal {

extern const char kHistogramPrerenderNavigationToActivation[];
extern const char kHistogramPrerenderActivationToFirstPaint[];
extern const char kHistogramPrerenderActivationToFirstContentfulPaint[];
extern const char kHistogramPrerenderActivationToLargestContentfulPaint2[];
extern const char kHistogramPrerenderFirstInputDelay4[];
extern const char kHistogramPrerenderCumulativeShiftScore[];
extern const char kHistogramPrerenderCumulativeShiftScoreMainFrame[];
extern const char
    kHistogramPrerenderMaxCumulativeShiftScoreSessionWindowGap1000msMax5000ms2
        [];

// Responsiveness metrics.
extern const char
    kHistogramPrerenderAverageUserInteractionLatencyOverBudgetMaxEventDuration
        [];
extern const char kHistogramPrerenderNumInteractions[];
extern const char
    kHistogramPrerenderUserInteractionLatencyHighPercentile2MaxEventDuration[];
extern const char
    kHistogramPrerenderWorstUserInteractionLatencyMaxEventDuration[];

}  // namespace internal

namespace page_load_metrics {
struct ExtraRequestCompleteInfo;
}  // namespace page_load_metrics

namespace net {
enum Error;
}  // namespace net

// Prerender2 (content/browser/preloading/prerender/README.md):
// Records custom page load timing and loading status metrics for prerendered
// page loads.
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
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordSessionEndHistograms(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);
  // Records Cumulative Layout Shift Score (CLS) to UMA and UKM.
  void RecordLayoutShiftScoreMetrics(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);
  // Records Interaction to Next Paint (INP) to UMA and UKM.
  void RecordNormalizedResponsivenessMetrics();

  // Records loading status for an activated and loaded page.
  void MaybeRecordMainResourceLoadStatus();

  // Helper function to concatenate the histogram name, the trigger type and the
  // embedder histogram suffix when the trigger type is kEmbedder.
  std::string AppendSuffix(const std::string& histogram_name) const;

  // Set to true if the activation navigation main frame resource has a
  // 'Cache-control: no-store' response header and set to false otherwise. Not
  // set if Chrome did not receive response headers or if the prerendered page
  // load was not activated.
  std::optional<bool> main_frame_resource_has_no_store_;

  // Set when the main resource of the main frame finishes loading.
  std::optional<net::Error> main_resource_load_status_;

  // The type to trigger prerendering.
  std::optional<content::PreloadingTriggerType> trigger_type_;
  // The suffix of a prerender embedder. This value is valid only when
  // PreloadingTriggerType is kEmbedder. Otherwise, it's an empty string.
  std::string embedder_histogram_suffix_;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRERENDER_PAGE_LOAD_METRICS_OBSERVER_H_

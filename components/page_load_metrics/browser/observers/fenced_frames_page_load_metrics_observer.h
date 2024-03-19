// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_FENCED_FRAMES_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_FENCED_FRAMES_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramFencedFramesNavigationToFirstPaint[];
extern const char kHistogramFencedFramesNavigationToFirstImagePaint[];
extern const char kHistogramFencedFramesNavigationToFirstContentfulPaint[];
extern const char kHistogramFencedFramesNavigationToLargestContentfulPaint2[];
extern const char kHistogramFencedFramesFirstInputDelay4[];
extern const char kHistogramFencedFramesCumulativeShiftScore[];
extern const char kHistogramFencedFramesCumulativeShiftScoreMainFrame[];

}  // namespace internal

// FencedFramesPageLoadMetricsObserver records UMA histograms prefixed by
// "PageLoad.Clients.FencedFrames" that contains information inside a
// FencedFrames. See https://github.com/shivanigithub/fenced-frame  to learn
// FencedFrames details.
class FencedFramesPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  FencedFramesPageLoadMetricsObserver() = default;
  ~FencedFramesPageLoadMetricsObserver() override = default;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstImagePaintInPage(
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

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_FENCED_FRAMES_PAGE_LOAD_METRICS_OBSERVER_H_

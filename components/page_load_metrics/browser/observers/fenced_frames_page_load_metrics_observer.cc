// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/fenced_frames_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"

namespace internal {

const char kHistogramFencedFramesNavigationToFirstPaint[] =
    "PageLoad.Clients.FencedFrames.PaintTiming.NavigationToFirstPaint";
const char kHistogramFencedFramesNavigationToFirstImagePaint[] =
    "PageLoad.Clients.FencedFrames.PaintTiming.NavigationToFirstImagePaint";
const char kHistogramFencedFramesNavigationToFirstContentfulPaint[] =
    "PageLoad.Clients.FencedFrames.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramFencedFramesNavigationToLargestContentfulPaint2[] =
    "PageLoad.Clients.FencedFrames.PaintTiming."
    "NavigationToLargestContentfulPaint2";
const char kHistogramFencedFramesFirstInputDelay4[] =
    "PageLoad.Clients.FencedFrames.InteractiveTiming.FirstInputDelay4";
const char kHistogramFencedFramesCumulativeShiftScore[] =
    "PageLoad.Clients.FencedFrames.LayoutInstability.CumulativeShiftScore";
const char kHistogramFencedFramesCumulativeShiftScoreMainFrame[] =
    "PageLoad.Clients.FencedFrames.LayoutInstability.CumulativeShiftScore."
    "MainFrame";

}  // namespace internal

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FencedFramesPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  // This class is not interested in primary pages. Existing observers already
  // record several metrics. Just stop observing here to focus on FencedFrames.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FencedFramesPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Receive events for FencedFrames.
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FencedFramesPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Pages that contain FencedFrames are not eligible for prerendering.
  // TODO(crbug.com/40228553): Make those pages prerenderable.
  return STOP_OBSERVING;
}

void FencedFramesPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFencedFramesNavigationToFirstPaint,
                        timing.paint_timing->first_paint.value());
  }
}

void FencedFramesPageLoadMetricsObserver::OnFirstImagePaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_image_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramFencedFramesNavigationToFirstImagePaint,
        timing.paint_timing->first_image_paint.value());
  }
}

void FencedFramesPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramFencedFramesNavigationToFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value());
  }
}

void FencedFramesPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    base::UmaHistogramCustomTimes(
        internal::kHistogramFencedFramesFirstInputDelay4,
        timing.interactive_timing->first_input_delay.value(),
        base::Milliseconds(1), base::Seconds(60), 50);
  }
}

void FencedFramesPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FencedFramesPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
  return STOP_OBSERVING;
}

void FencedFramesPageLoadMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    base::UmaHistogramCustomTimes(
        internal::kHistogramFencedFramesNavigationToLargestContentfulPaint2,
        largest_contentful_paint.Time().value(), base::Milliseconds(10),
        base::Minutes(10), 100);
  }

  base::UmaHistogramCounts100(
      internal::kHistogramFencedFramesCumulativeShiftScore,
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetPageRenderData().layout_shift_score));
  base::UmaHistogramCounts100(
      internal::kHistogramFencedFramesCumulativeShiftScoreMainFrame,
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetMainFrameRenderData().layout_shift_score));
}

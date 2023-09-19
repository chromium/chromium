// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/cross_origin_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

namespace {

constexpr char kHistogramCrossOriginFirstContentfulPaint[] =
    "PageLoad.Clients.CrossOrigin.FirstContentfulPaint";
constexpr char kHistogramCrossOriginLargestContentfulPaint[] =
    "PageLoad.Clients.CrossOrigin.LargestContentfulPaint";
constexpr char kHistogramCrossOriginFirstInputDelay[] =
    "PageLoad.Clients.CrossOrigin.InteractiveTiming.FirstInputDelay";
constexpr char kHistogramCrossOriginCumulativeLayoutShiftScore[] =
    "PageLoad.Clients.CrossOrigin.LayoutInstability.CumulativeShiftScore";
constexpr char kHistogramCrossOriginCumulativeLayoutShiftMainFrame[] =
    "PageLoad.Clients.CrossOrigin.LayoutInstability.CumulativeShiftScore."
    "MainFrame";

}  // namespace

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CrossOriginPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CrossOriginPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This follows UmaPageLoadMetricsObserver.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CrossOriginPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsSameOrigin()) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

void CrossOriginPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(kHistogramCrossOriginFirstContentfulPaint,
                      timing.paint_timing->first_contentful_paint.value());
}

void CrossOriginPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    return;
  }

  INPUT_DELAY_HISTOGRAM(kHistogramCrossOriginFirstInputDelay,
                        timing.interactive_timing->first_input_delay.value());
}

void CrossOriginPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordTimingHistograms();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CrossOriginPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // This follows UmaPageLoadMetricsObserver.
  if (GetDelegate().DidCommit()) {
    RecordTimingHistograms();
  }
  return STOP_OBSERVING;
}

void CrossOriginPageLoadMetricsObserver::RecordTimingHistograms() {
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(kHistogramCrossOriginLargestContentfulPaint,
                        largest_contentful_paint.Time().value());
  }

  base::UmaHistogramCounts100(
      kHistogramCrossOriginCumulativeLayoutShiftScore,
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetPageRenderData().layout_shift_score));
  base::UmaHistogramCounts100(
      kHistogramCrossOriginCumulativeLayoutShiftMainFrame,
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetMainFrameRenderData().layout_shift_score));
}

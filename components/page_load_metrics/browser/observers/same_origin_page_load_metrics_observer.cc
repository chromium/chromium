// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/same_origin_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

namespace {

constexpr char kHistogramSameOriginFirstContentfulPaint[] =
    "PageLoad.Clients.SameOrigin.FirstContentfulPaint";
constexpr char kHistogramSameOriginLargestContentfulPaint[] =
    "PageLoad.Clients.SameOrigin.LargestContentfulPaint";
constexpr char kHistogramSameOriginFirstInputDelay[] =
    "PageLoad.Clients.SameOrigin.InteractiveTiming.FirstInputDelay";
constexpr char kHistogramSameOriginCumulativeLayoutShiftScore[] =
    "PageLoad.Clients.SameOrigin.LayoutInstability.CumulativeShiftScore";
constexpr char kHistogramSameOriginCumulativeLayoutShiftMainFrame[] =
    "PageLoad.Clients.SameOrigin.LayoutInstability.CumulativeShiftScore."
    "MainFrame";

}  // namespace

SameOriginPageLoadMetricsObserver::SameOriginPageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SameOriginPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SameOriginPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This follows UmaPageLoadMetricsObserver.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SameOriginPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameOrigin()) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

void SameOriginPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(kHistogramSameOriginFirstContentfulPaint,
                      timing.paint_timing->first_contentful_paint.value());
}

void SameOriginPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    return;
  }

  INPUT_DELAY_HISTOGRAM(kHistogramSameOriginFirstInputDelay,
                        timing.interactive_timing->first_input_delay.value());
}

void SameOriginPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordTimingHistograms();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SameOriginPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // This follows UmaPageLoadMetricsObserver.
  if (GetDelegate().DidCommit()) {
    RecordTimingHistograms();
  }
  return STOP_OBSERVING;
}

void SameOriginPageLoadMetricsObserver::RecordTimingHistograms() {
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(kHistogramSameOriginLargestContentfulPaint,
                        largest_contentful_paint.Time().value());
  }

  base::UmaHistogramCounts100(
      kHistogramSameOriginCumulativeLayoutShiftScore,
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetPageRenderData().layout_shift_score));
  base::UmaHistogramCounts100(
      kHistogramSameOriginCumulativeLayoutShiftMainFrame,
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetMainFrameRenderData().layout_shift_score));
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/prerender_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace internal {

const char kHistogramPrerenderNavigationToActivation[] =
    "PageLoad.Clients.Prerender.NavigationToActivation";
const char kHistogramPrerenderActivationToFirstPaint[] =
    "PageLoad.Clients.Prerender.PaintTiming.ActivationToFirstPaint";
const char kHistogramPrerenderActivationToFirstContentfulPaint[] =
    "PageLoad.Clients.Prerender.PaintTiming.ActivationToFirstContentfulPaint";
const char kHistogramPrerenderActivationToLargestContentfulPaint2[] =
    "PageLoad.Clients.Prerender.PaintTiming."
    "ActivationToLargestContentfulPaint2";
const char kHistogramPrerenderFirstInputDelay4[] =
    "PageLoad.Clients.Prerender.InteractiveTiming.FirstInputDelay4";
const char kHistogramPrerenderCumulativeShiftScore[] =
    "PageLoad.Clients.Prerender.LayoutInstability.CumulativeShiftScore";
const char kHistogramPrerenderCumulativeShiftScoreMainFrame[] =
    "PageLoad.Clients.Prerender.LayoutInstability.CumulativeShiftScore."
    "MainFrame";

}  // namespace internal

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrerenderPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrerenderPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return STOP_OBSERVING;
}

void PrerenderPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  // |navigation_handle| here is for the activation navigation, while
  // |GetDelegate().GetNavigationStart()| is the start time of initial prerender
  // navigation.
  base::TimeDelta navigation_to_activation =
      navigation_handle->NavigationStart() - GetDelegate().GetNavigationStart();
  PAGE_LOAD_HISTOGRAM(internal::kHistogramPrerenderNavigationToActivation,
                      navigation_to_activation);

  ukm::builders::PrerenderPageLoad(GetDelegate().GetPageUkmSourceId())
      .SetWasPrerendered(true)
      .SetTiming_NavigationToActivation(
          navigation_to_activation.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void PrerenderPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!WasActivatedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(internal::kHistogramPrerenderActivationToFirstPaint,
                      timing.paint_timing->first_paint.value() -
                          timing.activation_start.value());
}

void PrerenderPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!WasActivatedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }
  base::TimeDelta activation_to_fcp =
      timing.paint_timing->first_contentful_paint.value() -
      timing.activation_start.value();
  PAGE_LOAD_HISTOGRAM(
      internal::kHistogramPrerenderActivationToFirstContentfulPaint,
      activation_to_fcp);
  ukm::builders::PrerenderPageLoad(GetDelegate().GetPageUkmSourceId())
      .SetTiming_ActivationToFirstContentfulPaint(
          activation_to_fcp.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void PrerenderPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!WasActivatedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    return;
  }
  UMA_HISTOGRAM_CUSTOM_TIMES(
      internal::kHistogramPrerenderFirstInputDelay4,
      timing.interactive_timing->first_input_delay.value(),
      base::Milliseconds(1), base::Seconds(60), 50);
}

void PrerenderPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrerenderPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
  return STOP_OBSERVING;
}

void PrerenderPageLoadMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  if (!GetDelegate().WasPrerenderedThenActivatedInForeground() ||
      !main_frame_timing.activation_start) {
    // Even if the page was activated, activation_start may not yet been
    // notified by the renderer. Ignore such page loads.
    return;
  }

  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      WasActivatedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    base::TimeDelta activation_to_lcp =
        largest_contentful_paint.Time().value() -
        main_frame_timing.activation_start.value();
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramPrerenderActivationToLargestContentfulPaint2,
        activation_to_lcp);
    ukm::builders::PrerenderPageLoad(GetDelegate().GetPageUkmSourceId())
        .SetTiming_ActivationToLargestContentfulPaint(
            activation_to_lcp.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }

  UMA_HISTOGRAM_COUNTS_100(
      internal::kHistogramPrerenderCumulativeShiftScore,
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetPageRenderData().layout_shift_score));
  UMA_HISTOGRAM_COUNTS_100(
      internal::kHistogramPrerenderCumulativeShiftScoreMainFrame,
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetMainFrameRenderData().layout_shift_score));
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/early_hints_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

namespace internal {

const char kHistogramEarlyHintsPreloadFirstContentfulPaint[] =
    "PageLoad.Clients.EarlyHints.Preload.PaintTiming."
    "NavigationToFirstContentfulPaint";

const char kHistogramEarlyHintsPreloadLargestContentfulPaint[] =
    "PageLoad.Clients.EarlyHints.Preload.PaintTiming."
    "NavigationToLargestContentfulPaint2";

const char kHistogramEarlyHintsPreloadFirstInputDelay[] =
    "PageLoad.Clients.EarlyHints.Preload.InteractiveTiming.FirstInputDelay4";

}  // namespace internal

EarlyHintsPageLoadMetricsObserver::EarlyHintsPageLoadMetricsObserver() =
    default;

EarlyHintsPageLoadMetricsObserver::~EarlyHintsPageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
EarlyHintsPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class doesn't use subframe information. No need to forward.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
EarlyHintsPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class works as same as non prerendering case except for the metrics
  // correction.
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
EarlyHintsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // Continue observing when 103 Early Hints are received during the navigation
  // and these contain at least one resource hints (preload or preconnect).
  if (navigation_handle->WasResourceHintsReceived())
    return CONTINUE_OBSERVING;
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
EarlyHintsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!GetDelegate().DidCommit())
    return STOP_OBSERVING;
  if (GetDelegate().IsInPrerenderingBeforeActivationStart())
    return STOP_OBSERVING;

  RecordHistograms(timing);

  return STOP_OBSERVING;
}

void EarlyHintsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().IsInPrerenderingBeforeActivationStart())
    return;

  RecordHistograms(timing);
}

void EarlyHintsPageLoadMetricsObserver::RecordHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // Record histograms only for pages that reach FCP/LCP/FID in foreground to
  // avoid skews.

  if (timing.paint_timing->first_contentful_paint.has_value() &&
      page_load_metrics::EventOccurredBeforeNonPrerenderingBackgroundStart(
          GetDelegate(), timing,
          timing.paint_timing->first_contentful_paint.value())) {
    base::TimeDelta corrected =
        page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
            GetDelegate(), timing,
            timing.paint_timing->first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramEarlyHintsPreloadFirstContentfulPaint, corrected);
  }

  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MainFrameLargestContentfulPaint();
  if (largest_contentful_paint.ContainsValidTime() &&
      page_load_metrics::EventOccurredBeforeNonPrerenderingBackgroundStart(
          GetDelegate(), timing, largest_contentful_paint.Time().value())) {
    base::TimeDelta corrected =
        page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
            GetDelegate(), timing, largest_contentful_paint.Time().value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramEarlyHintsPreloadLargestContentfulPaint, corrected);
  }

  if (timing.interactive_timing->first_input_delay.has_value() &&
      page_load_metrics::EventOccurredBeforeNonPrerenderingBackgroundStart(
          GetDelegate(), timing,
          timing.interactive_timing->first_input_delay.value())) {
    base::UmaHistogramCustomTimes(
        internal::kHistogramEarlyHintsPreloadFirstInputDelay,
        timing.interactive_timing->first_input_delay.value(),
        base::Milliseconds(1), base::Seconds(60), 50);
  }
}

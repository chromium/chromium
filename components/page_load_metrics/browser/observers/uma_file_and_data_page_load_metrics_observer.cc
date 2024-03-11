// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/uma_file_and_data_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace {
const char kHistogramFirstContentfulPaintDataScheme[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.DataScheme";
const char kHistogramFirstContentfulPaintFileScheme[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.FileScheme";
const char kBackgroundHttpsOrDataOrFileSchemeHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background."
    "HttpsOrDataOrFileScheme";
const char kHistogramLargestContentfulPaintDataScheme[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.DataScheme";
const char kHistogramLargestContentfulPaintFileScheme[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.FileScheme";
const char kBackgroundHttpsOrDataOrFileSchemeHistogramLargestContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Background."
    "HttpsOrDataOrFileScheme";
}  // namespace

void UmaFileAndDataPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(timing.paint_timing->first_contentful_paint);
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    if (GetDelegate().GetUrl().SchemeIs(url::kDataScheme)) {
      PAGE_LOAD_HISTOGRAM(kHistogramFirstContentfulPaintDataScheme,
                          timing.paint_timing->first_contentful_paint.value());
      return;
    }

    if (GetDelegate().GetUrl().SchemeIs(url::kFileScheme)) {
      PAGE_LOAD_HISTOGRAM(kHistogramFirstContentfulPaintFileScheme,
                          timing.paint_timing->first_contentful_paint.value());
      return;
    }
  } else {
    PAGE_LOAD_HISTOGRAM(
        kBackgroundHttpsOrDataOrFileSchemeHistogramFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value());
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UmaFileAndDataPageLoadMetricsObserver::ShouldObserveScheme(
    const GURL& url) const {
  if (url.SchemeIs(url::kFileScheme) || url.SchemeIs(url::kDataScheme)) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UmaFileAndDataPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UmaFileAndDataPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

void UmaFileAndDataPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordTimingHistograms();
}

void UmaFileAndDataPageLoadMetricsObserver::RecordTimingHistograms() {
  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (!all_frames_largest_contentful_paint.ContainsValidTime()) {
    return;
  }

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    if (GetDelegate().GetUrl().SchemeIs(url::kDataScheme)) {
      PAGE_LOAD_HISTOGRAM(kHistogramLargestContentfulPaintDataScheme,
                          all_frames_largest_contentful_paint.Time().value());
      return;
    }

    if (GetDelegate().GetUrl().SchemeIs(url::kFileScheme)) {
      PAGE_LOAD_HISTOGRAM(kHistogramLargestContentfulPaintFileScheme,
                          all_frames_largest_contentful_paint.Time().value());
      return;
    }
  } else {
    PAGE_LOAD_HISTOGRAM(
        kBackgroundHttpsOrDataOrFileSchemeHistogramLargestContentfulPaint,
        all_frames_largest_contentful_paint.Time().value());
  }
}

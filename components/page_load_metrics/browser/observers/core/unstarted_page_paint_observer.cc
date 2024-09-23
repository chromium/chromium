// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/core/unstarted_page_paint_observer.h"

#include "base/metrics/histogram_functions.h"

namespace internal {

const char kPageLoadUnstartedPagePaint[] =
    "PageLoad.Clients.NavigationToFirstContentfulPaint.Timeout";

}  // namespace internal

void RecordUnstartedPagePaint(bool timeout_expired) {
  base::UmaHistogramBoolean(internal::kPageLoadUnstartedPagePaint,
                            timeout_expired);
}

const char* UnstartedPagePaintObserver::GetObserverName() const {
  static const char kName[] = "UnstartedPagePaintObserver";
  return kName;
}

UnstartedPagePaintObserver::UnstartedPagePaintObserver() = default;

UnstartedPagePaintObserver::~UnstartedPagePaintObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UnstartedPagePaintObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  StartUnstartedPagePaintTimer();

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UnstartedPagePaintObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UnstartedPagePaintObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

void UnstartedPagePaintObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  StopUnstartedPagePaintTimer(true);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UnstartedPagePaintObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // Stop unstarted page paint timer in case that page enters BackForwardCache.
  StopUnstartedPagePaintTimer(false);

  return CONTINUE_OBSERVING;
}

void UnstartedPagePaintObserver::OnRestoreFromBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    content::NavigationHandle* navigation_handle) {
  const page_load_metrics::mojom::PaintTimingPtr& paint_timing =
      timing.paint_timing;

  // Start unstarted page paint timer when page restoring from
  // BackForwardCache and first contentful paint was not yet reported.
  CHECK(!navigation_timeout_timer_.IsRunning());
  if (!paint_timing->first_contentful_paint.has_value()) {
    StartUnstartedPagePaintTimer();
  }
}

void UnstartedPagePaintObserver::StartUnstartedPagePaintTimer() {
  StopUnstartedPagePaintTimer(false);

  navigation_timeout_timer_.Start(
      FROM_HERE, base::Seconds(internal::kUnstartedPagePaintTimeoutSeconds),
      base::BindOnce(&UnstartedPagePaintObserver::OnUnstartedPagePaintExpired,
                     weak_factory_.GetWeakPtr()));
}

void UnstartedPagePaintObserver::StopUnstartedPagePaintTimer(
    bool first_content_paint) {
  if (navigation_timeout_timer_.IsRunning()) {
    if (first_content_paint) {
      RecordUnstartedPagePaint(false);
    }
    navigation_timeout_timer_.Stop();
  }
}

void UnstartedPagePaintObserver::OnUnstartedPagePaintExpired() const {
  RecordUnstartedPagePaint(true);
}

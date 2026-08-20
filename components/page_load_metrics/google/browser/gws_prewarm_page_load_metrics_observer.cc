// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_prewarm_page_load_metrics_observer.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "content/public/browser/navigation_handle.h"

namespace internal {

const char kHistogramGWSPrewarmWasResponseCached[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.WasResponseCached";

}  // namespace internal

GWSPrewarmPageLoadMetricsObserver::GWSPrewarmPageLoadMetricsObserver() =
    default;

GWSPrewarmPageLoadMetricsObserver::~GWSPrewarmPageLoadMetricsObserver() =
    default;

const char* GWSPrewarmPageLoadMetricsObserver::GetObserverName() const {
  static const char kObserverName[] = "GWSPrewarmPageLoadMetricsObserver";
  return kObserverName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPrewarmPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  // Prewarm pages are only ever loaded via prerendering.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPrewarmPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  if (!page_load_metrics::IsGoogleSearchPrewarmUrl(
          navigation_handle->GetURL())) {
    return STOP_OBSERVING;
  }
  is_prerendered_ = true;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPrewarmPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSPrewarmPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  CHECK(is_prerendered_);
  if (!page_load_metrics::IsGoogleSearchPrewarmUrl(
          navigation_handle->GetURL())) {
    return STOP_OBSERVING;
  }
  was_cached_ = navigation_handle->WasResponseCached();
  base::UmaHistogramBoolean(internal::kHistogramGWSPrewarmWasResponseCached,
                            was_cached_);
  return CONTINUE_OBSERVING;
}

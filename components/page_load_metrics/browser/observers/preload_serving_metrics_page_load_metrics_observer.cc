// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/preload_serving_metrics_page_load_metrics_observer.h"

#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

PreloadServingMetricsPageLoadMetricsObserver::
    PreloadServingMetricsPageLoadMetricsObserver() = default;

PreloadServingMetricsPageLoadMetricsObserver::
    ~PreloadServingMetricsPageLoadMetricsObserver() = default;

const char* PreloadServingMetricsPageLoadMetricsObserver::GetObserverName()
    const {
  static const char kName[] = "PreloadServingMetricsPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

void PreloadServingMetricsPageLoadMetricsObserver::
    RetrieveNavigationInitiatorLocationAndSrp(
        content::NavigationHandle* navigation_handle) {
  auto* user_data =
      page_load_metrics::NavigationHandleUserData::GetForNavigationHandle(
          *navigation_handle);
  if (user_data) {
    navigation_initiator_string_ = user_data->navigation_type_string();
  } else {
    navigation_initiator_string_ = "Other";
  }
  is_url_srp_ = google_util::IsGoogleSearchUrl(navigation_handle->GetURL());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return STOP_OBSERVING;
  }

  if (navigation_handle->IsInPrerenderedMainFrame()) {
    // Wait prerender activation.
    return CONTINUE_OBSERVING;
  }

  RetrieveNavigationInitiatorLocationAndSrp(navigation_handle);

  // Take `PreloadServingMetrics` of non prerender navigation.
  preload_serving_metrics_capsule_ =
      content::PreloadServingMetricsCapsule::TakeFromNavigationHandle(
          *navigation_handle);
  CHECK(preload_serving_metrics_capsule_);

  return CONTINUE_OBSERVING;
}

void PreloadServingMetricsPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  RetrieveNavigationInitiatorLocationAndSrp(navigation_handle);

  // Take `PreloadServingMetrics` of prerender activation navigation.
  preload_serving_metrics_capsule_ =
      content::PreloadServingMetricsCapsule::TakeFromNavigationHandle(
          *navigation_handle);
  CHECK(preload_serving_metrics_capsule_);
}

void PreloadServingMetricsPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // Note that
  // `preload_serving_metrics_capsule_` can be null if the page entered
  // BackForwardCache before FCP occurred (which resets the capsule) or if FCP
  // timing is delivered out of order.
  if (has_entered_bfcache_) {
    return;
  }

  // `OnFirstContentfulPaintInPage()` is called after `OnCommit()` (or
  // `DidActivatePrerenderedPage()` for prerender).
  CHECK(preload_serving_metrics_capsule_);

  base::TimeDelta corrected =
      page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
          GetDelegate(), timing.paint_timing->first_contentful_paint.value());
  preload_serving_metrics_capsule_->RecordFirstContentfulPaint(
      std::move(corrected));
}

void PreloadServingMetricsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  MaybeRecord();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  MaybeRecord();
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreloadServingMetricsPageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  MaybeRecord();
  has_entered_bfcache_ = true;
  preload_serving_metrics_capsule_.reset();
  return CONTINUE_OBSERVING;
}

void PreloadServingMetricsPageLoadMetricsObserver::
    OnRestoreFromBackForwardCache(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        content::NavigationHandle* navigation_handle) {
  has_restored_from_bfcache_ = true;
  RetrieveNavigationInitiatorLocationAndSrp(navigation_handle);
  // Take a PreloadServingMetrics of the NavigationHandle representing the
  // navigation that used BFCache. Note that the NavigationHandle differs from
  // the one created this PLMO, and the PreloadServingMetrics for it has been
  // reset.
  preload_serving_metrics_capsule_ =
      content::PreloadServingMetricsCapsule::TakeFromNavigationHandle(
          *navigation_handle);
}

void PreloadServingMetricsPageLoadMetricsObserver::MaybeRecord() {
  // Record if the navigation is non prerender and committed; or if the
  // navigations are prerender initial/activation navigation and activated.

  if (!preload_serving_metrics_capsule_) {
    return;
  }

  CHECK(navigation_initiator_string_.has_value());

  preload_serving_metrics_capsule_
      ->RecordMetricsForNonPrerenderNavigationCommitted();
  // TODO(https://crbug.com/517725655): PreloadServingMetricsCapsule is taken
  // for BFCache, and this part should be re-visited again to explore better
  // ways to record this case.
  preload_serving_metrics_capsule_
      ->RecordPreloadServingMetricsByNavigationInitiator(
          has_restored_from_bfcache_, *navigation_initiator_string_,
          is_url_srp_);
}

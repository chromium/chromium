// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/preload_serving_metrics_page_load_metrics_observer.h"

#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"

namespace {

std::string GetNavigationInitiatorString(
    content::NavigationHandle* navigation_handle) {
  if (ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_RELOAD)) {
    return "Reload";
  }

  if ((navigation_handle->GetPageTransition() &
       ui::PAGE_TRANSITION_FORWARD_BACK) ||
      navigation_handle->IsServedFromBackForwardCache()) {
    int history_offset = navigation_handle->GetNavigationEntryOffset();
    if (history_offset > 0) {
      return "Forward";
    }

    if (history_offset < 0) {
      return "Backward";
    }

    return "Other";
  }

  auto* user_data =
      page_load_metrics::NavigationHandleUserData::GetForNavigationHandle(
          *navigation_handle);
  if (user_data) {
    return user_data->navigation_type_string();
  }

  if (navigation_handle->IsRendererInitiated() &&
      navigation_handle->HasUserGesture() &&
      ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_LINK)) {
    return "LinkClick";
  }

  return "Other";
}

}  // namespace

PreloadServingMetricsPageLoadMetricsObserver::NavigationData::NavigationData() =
    default;
PreloadServingMetricsPageLoadMetricsObserver::NavigationData::
    ~NavigationData() = default;
PreloadServingMetricsPageLoadMetricsObserver::NavigationData::NavigationData(
    NavigationData&&) = default;
PreloadServingMetricsPageLoadMetricsObserver::NavigationData&
PreloadServingMetricsPageLoadMetricsObserver::NavigationData::operator=(
    NavigationData&&) = default;

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

PreloadServingMetricsPageLoadMetricsObserver::NavigationData
PreloadServingMetricsPageLoadMetricsObserver::CreateNavigationData(
    content::NavigationHandle* navigation_handle,
    bool used_bfcache) {
  NavigationData navigation_data;
  navigation_data.preload_serving_metrics_capsule =
      content::PreloadServingMetricsCapsule::TakeFromNavigationHandle(
          *navigation_handle);
  CHECK(navigation_data.preload_serving_metrics_capsule);

  navigation_data.used_bfcache = used_bfcache;
  navigation_data.navigation_initiator_string =
      GetNavigationInitiatorString(navigation_handle);
  navigation_data.is_url_srp =
      google_util::IsGoogleSearchUrl(navigation_handle->GetURL());

  return navigation_data;
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

  navigation_data_ =
      CreateNavigationData(navigation_handle, /*used_bfcache=*/false);

  return CONTINUE_OBSERVING;
}

void PreloadServingMetricsPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  navigation_data_ =
      CreateNavigationData(navigation_handle, /*used_bfcache=*/false);
}

void PreloadServingMetricsPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // `navigation_data_` can be null if the page entered BackForwardCache before
  // FCP occurred (which resets `navigation_data_`) or if FCP timing is
  // delivered out of order.
  //
  // TODO(https://crbug.com/539388005): `PLMO::OnFirstContentfulPaintInPage()`
  // is not expected to be called between `PLMO::OnEnterBackForwardCache()` and
  // `PLMO::OnRestoreFromBackForwardCache()`, but currently it is happening.
  if (!navigation_data_) {
    return;
  }

  const bool is_prerender =
      GetDelegate().GetPrerenderingState() !=
      page_load_metrics::PrerenderingState::kNoPrerendering;
  const bool is_in_foreground =
      is_prerender
          ? page_load_metrics::
                WasActivatedInForegroundOptionalEventInForeground(
                    timing.paint_timing->first_contentful_paint, GetDelegate())
          : page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
                timing.paint_timing->first_contentful_paint, GetDelegate());

  base::TimeDelta corrected =
      page_load_metrics::CorrectEventAsNavigationOrActivationOrigined(
          GetDelegate(), timing.paint_timing->first_contentful_paint.value());
  navigation_data_->preload_serving_metrics_capsule->RecordFirstContentfulPaint(
      corrected, is_in_foreground,
      navigation_data_->navigation_initiator_string,
      navigation_data_->is_url_srp);
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
  navigation_data_.reset();
  return CONTINUE_OBSERVING;
}

void PreloadServingMetricsPageLoadMetricsObserver::
    OnRestoreFromBackForwardCache(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        content::NavigationHandle* navigation_handle) {
  navigation_data_ =
      CreateNavigationData(navigation_handle, /*used_bfcache=*/true);
}

void PreloadServingMetricsPageLoadMetricsObserver::MaybeRecord() {
  // Record if the navigation is non prerender and committed; or if the
  // navigations are prerender initial/activation navigation and activated.

  if (!navigation_data_) {
    return;
  }

  navigation_data_->preload_serving_metrics_capsule
      ->RecordMetricsForNonPrerenderNavigationCommitted();
  // TODO(https://crbug.com/517725655): PreloadServingMetricsCapsule is taken
  // for BFCache, and this part should be re-visited again to explore better
  // ways to record this case.
  navigation_data_->preload_serving_metrics_capsule
      ->RecordPreloadServingMetricsByNavigationInitiator(
          navigation_data_->used_bfcache,
          navigation_data_->navigation_initiator_string,
          navigation_data_->is_url_srp);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_prewarm_page_load_metrics_observer.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"

namespace internal {

const char kHistogramGWSPrewarmWasResponseCached[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.WasResponseCached";
const char kHistogramGWSPrewarmWasActivated[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.WasActivated";
const char kHistogramGWSPrewarmNavigationToCancellation[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.NavigationToCancellation";
const char kHistogramGWSPrewarmNavigationToDomContentLoaded[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.NavigationToDOMContentLoaded";
const char kHistogramGWSPrewarmNavigationToLoadEvent[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.NavigationToLoadEvent";
const char kHistogramGWSPrewarmLoadEventOccurredBeforeCancellation[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.LoadEventOccurredBeforeCancellation";
const char kHistogramGWSPrewarmLoadEventToCancellation[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.LoadEventToCancellation";
const char kHistogramGWSPrewarmNavigationToLastSubresourceLoad[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.NavigationToLastSubresourceLoad";
const char kHistogramGWSPrewarmLastSubresourceLoadToCancellation[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.LastSubresourceLoadToCancellation";
const char kHistogramGWSPrewarmSubresourceDestination[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.SubresourceDestination";
const char kHistogramGWSPrewarmSubresourceCount[] =
    "PageLoad.Clients.GoogleSearch.Prewarm.SubresourceCount";

const char kSubresourceSuffixCached[] = ".Cached";
const char kSubresourceSuffixHttpCached[] = ".HttpCached";
const char kSubresourceSuffixMemoryCached[] = ".MemoryCached";
const char kSubresourceSuffixNotCached[] = ".NotCached";

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
GWSPrewarmPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  if (!page_load_metrics::IsGoogleSearchPrewarmUrl(
          navigation_handle->GetURL())) {
    return STOP_OBSERVING;
  }
  return CONTINUE_OBSERVING;
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

void GWSPrewarmPageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (was_activated_) {
    return;
  }
  if (timing.document_timing &&
      timing.document_timing->dom_content_loaded_event_start.has_value() &&
      !timing.document_timing->dom_content_loaded_event_start->is_negative()) {
    PAGE_LOAD_HISTOGRAM2(
        internal::kHistogramGWSPrewarmNavigationToDomContentLoaded,
        timing.document_timing->dom_content_loaded_event_start.value());
  }
}

void GWSPrewarmPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (was_activated_) {
    return;
  }
  if (timing.document_timing &&
      timing.document_timing->load_event_start.has_value() &&
      !timing.document_timing->load_event_start->is_negative()) {
    load_event_start_ = timing.document_timing->load_event_start.value();
    PAGE_LOAD_HISTOGRAM2(internal::kHistogramGWSPrewarmNavigationToLoadEvent,
                         *load_event_start_);
  }
}

void GWSPrewarmPageLoadMetricsObserver::RecordSubresourceHistograms(
    network::mojom::RequestDestination request_destination,
    SubresourceCacheType cache_type,
    std::optional<base::TimeDelta> completion_time) {
  if (was_activated_) {
    return;
  }

  // Exclude main frame document resource load.
  if (request_destination == network::mojom::RequestDestination::kDocument) {
    return;
  }

  total_subresources_count_++;
  base::UmaHistogramEnumeration(
      internal::kHistogramGWSPrewarmSubresourceDestination,
      request_destination);

  if (cache_type != SubresourceCacheType::kNotCached) {
    cached_subresources_count_++;
    base::UmaHistogramEnumeration(
        base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                      internal::kSubresourceSuffixCached}),
        request_destination);
  }

  switch (cache_type) {
    case SubresourceCacheType::kNotCached:
      not_cached_subresources_count_++;
      base::UmaHistogramEnumeration(
          base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                        internal::kSubresourceSuffixNotCached}),
          request_destination);
      break;
    case SubresourceCacheType::kHttpCached:
      http_cached_subresources_count_++;
      base::UmaHistogramEnumeration(
          base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                        internal::kSubresourceSuffixHttpCached}),
          request_destination);
      break;
    case SubresourceCacheType::kMemoryCached:
      memory_cached_subresources_count_++;
      base::UmaHistogramEnumeration(
          base::StrCat({internal::kHistogramGWSPrewarmSubresourceDestination,
                        internal::kSubresourceSuffixMemoryCached}),
          request_destination);
      break;
  }

  if (completion_time.has_value() && !completion_time->is_negative()) {
    last_subresource_load_time_ =
        std::max(last_subresource_load_time_.value_or(base::TimeDelta()),
                 *completion_time);
  }
}

void GWSPrewarmPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (extra_request_complete_info.net_error != net::OK) {
    return;
  }

  std::optional<base::TimeDelta> completion_time;
  if (extra_request_complete_info.load_timing_info &&
      !extra_request_complete_info.load_timing_info->receive_headers_end
           .is_null()) {
    completion_time =
        extra_request_complete_info.load_timing_info->receive_headers_end -
        GetDelegate().GetNavigationStart();
  }

  RecordSubresourceHistograms(extra_request_complete_info.request_destination,
                              extra_request_complete_info.was_cached
                                  ? SubresourceCacheType::kHttpCached
                                  : SubresourceCacheType::kNotCached,
                              completion_time);
}

void GWSPrewarmPageLoadMetricsObserver::DidLoadResourceFromMemoryCache(
    const page_load_metrics::MemoryResourceLoadInfo&
        memory_resource_load_info) {
  // TODO(crbug.com/497025031): In-renderer memory cache loads complete
  // synchronously in Blink without network latency. Since the IPC doesn't carry
  // a renderer timestamp, pass std::nullopt for completion_time for now to
  // avoid browser-side IPC delay noise from distorting the network headroom
  // calculation.
  RecordSubresourceHistograms(memory_resource_load_info.request_destination,
                              SubresourceCacheType::kMemoryCached,
                              /*completion_time=*/std::nullopt);
}

void GWSPrewarmPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  // GWS prewarm pages should not normally be activated. Track the state so
  // that RecordSessionEndHistograms can record the monitoring metric.
  was_activated_ = true;
}

void GWSPrewarmPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(GetDelegate().GetTimeToPageEnd());
}

void GWSPrewarmPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  RecordSessionEndHistograms(
      failed_provisional_load_info.time_to_failed_provisional_load);
}

void GWSPrewarmPageLoadMetricsObserver::RecordSessionEndHistograms(
    std::optional<base::TimeDelta> time_to_page_end) {
  base::UmaHistogramBoolean(internal::kHistogramGWSPrewarmWasActivated,
                            was_activated_);

  if (was_activated_) {
    return;
  }

  if (time_to_page_end.has_value() && !time_to_page_end->is_negative()) {
    PAGE_LOAD_HISTOGRAM2(internal::kHistogramGWSPrewarmNavigationToCancellation,
                         *time_to_page_end);

    if (load_event_start_.has_value() &&
        *load_event_start_ <= *time_to_page_end) {
      base::UmaHistogramBoolean(
          internal::kHistogramGWSPrewarmLoadEventOccurredBeforeCancellation,
          true);
      PAGE_LOAD_HISTOGRAM2(
          internal::kHistogramGWSPrewarmLoadEventToCancellation,
          *time_to_page_end - *load_event_start_);
    } else {
      base::UmaHistogramBoolean(
          internal::kHistogramGWSPrewarmLoadEventOccurredBeforeCancellation,
          false);
    }

    if (last_subresource_load_time_.has_value() &&
        *last_subresource_load_time_ <= *time_to_page_end) {
      PAGE_LOAD_HISTOGRAM2(
          internal::kHistogramGWSPrewarmNavigationToLastSubresourceLoad,
          *last_subresource_load_time_);
      PAGE_LOAD_HISTOGRAM2(
          internal::kHistogramGWSPrewarmLastSubresourceLoadToCancellation,
          *time_to_page_end - *last_subresource_load_time_);
    }
  }

  base::UmaHistogramCounts100(internal::kHistogramGWSPrewarmSubresourceCount,
                              total_subresources_count_);
  base::UmaHistogramCounts100(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixCached}),
      cached_subresources_count_);
  base::UmaHistogramCounts100(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixHttpCached}),
      http_cached_subresources_count_);
  base::UmaHistogramCounts100(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixMemoryCached}),
      memory_cached_subresources_count_);
  base::UmaHistogramCounts100(
      base::StrCat({internal::kHistogramGWSPrewarmSubresourceCount,
                    internal::kSubresourceSuffixNotCached}),
      not_cached_subresources_count_);
}

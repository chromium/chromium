// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"

namespace internal {

const char kGWSAbandonedPageLoadMetricsHistogramPrefix[] =
    "PageLoad.Clients.GoogleSearch.Leakage2.";
const char kSuffixWasNonSRP[] = ".WasNonSRP";

const char kGwsAFTStartMarkName[] = "SearchAFTStart";
const char kGwsAFTEndMarkName[] = "trigger:SearchAFTEnd";
const char kGwsHeaderChunkStartMarkName[] = "SearchHeadStart";
const char kGwsHeaderChunkEndMarkName[] = "SearchHeadEnd";
const char kGwsBodyChunkStartMarkName[] = "SearchBodyStart";
const char kGwsBodyChunkEndMarkName[] = "SearchBodyEnd";

}  // namespace internal

GWSAbandonedPageLoadMetricsObserver::GWSAbandonedPageLoadMetricsObserver() =
    default;

GWSAbandonedPageLoadMetricsObserver::~GWSAbandonedPageLoadMetricsObserver() =
    default;

const char* GWSAbandonedPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "GWSAbandonedPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
GWSAbandonedPageLoadMetricsObserver::OnNavigationEvent(
    content::NavigationHandle* navigation_handle) {
  if (page_load_metrics::IsGoogleSearchResultUrl(navigation_handle->GetURL())) {
    involved_srp_url_ = true;
  } else {
    did_request_non_srp_ = true;

    if (!navigation_handle->GetNavigationHandleTiming()
             .non_redirect_response_start_time.is_null()) {
      // The navigation has received its final response, meaning that it can't
      // be redirected to SRP anymore, and the current URL is not SRP. As the
      // navigation didn't end up going to SRP, we shouldn't log any metric.
      return STOP_OBSERVING;
    }
  }

  return CONTINUE_OBSERVING;
}

const base::flat_map<std::string,
                     AbandonedPageLoadMetricsObserver::NavigationMilestone>&
GWSAbandonedPageLoadMetricsObserver::GetCustomUserTimingMarkNames() const {
  static const base::NoDestructor<
      base::flat_map<std::string, NavigationMilestone>>
      mark_names({
          {internal::kGwsAFTStartMarkName, NavigationMilestone::kAFTStart},
          {internal::kGwsAFTEndMarkName, NavigationMilestone::kAFTEnd},
          {internal::kGwsHeaderChunkStartMarkName,
           NavigationMilestone::kHeaderChunkStart},
          {internal::kGwsHeaderChunkEndMarkName,
           NavigationMilestone::kHeaderChunkEnd},
          {internal::kGwsBodyChunkStartMarkName,
           NavigationMilestone::kBodyChunkStart},
          {internal::kGwsBodyChunkEndMarkName,
           NavigationMilestone::kBodyChunkEnd},
      });
  return *mark_names;
}

bool GWSAbandonedPageLoadMetricsObserver::IsAllowedToLogMetrics() const {
  // Only log metrics for navigations that involve SRP.
  return involved_srp_url_;
}

bool GWSAbandonedPageLoadMetricsObserver::IsAllowedToLogUKM() const {
  // Only log UKMs for navigations that involve SRP.
  return involved_srp_url_;
}

std::string GWSAbandonedPageLoadMetricsObserver::GetHistogramPrefix() const {
  // Use the GWS-specific histograms.
  return internal::kGWSAbandonedPageLoadMetricsHistogramPrefix;
}

std::vector<std::string>
GWSAbandonedPageLoadMetricsObserver::GetAdditionalSuffixes() const {
  auto base_suffixes =
      AbandonedPageLoadMetricsObserver::GetAdditionalSuffixes();

  std::string request_source_suffix =
      did_request_non_srp_ ? internal::kSuffixWasNonSRP : "";
  std::vector<std::string> suffixes_with_request_source;
  // Add suffix that indicates the navigation prevviously requested a non-SRP
  // URL (instead of immediately targeting a SRP URL) to all histograms, if
  // necessary.
  for (auto base_suffix : base_suffixes) {
    suffixes_with_request_source.push_back(base_suffix + request_source_suffix);
  }
  return suffixes_with_request_source;
}

void GWSAbandonedPageLoadMetricsObserver::AddSRPMetricsToUKMIfNeeded(
    ukm::builders::AbandonedSRPNavigation& builder) {
  builder.SetDidRequestNonSRP(did_request_non_srp_);
}

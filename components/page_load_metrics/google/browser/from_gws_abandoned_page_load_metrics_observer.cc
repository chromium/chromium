// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/from_gws_abandoned_page_load_metrics_observer.h"

#include <string>

#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace {

const char kFromGWSAbandonedPageLoadMetricsHistogramPrefix[] =
    "PageLoad.Clients.FromGoogleSearch.Leakage2.";

}  // namespace

FromGWSAbandonedPageLoadMetricsObserver::
    FromGWSAbandonedPageLoadMetricsObserver() = default;

FromGWSAbandonedPageLoadMetricsObserver::
    ~FromGWSAbandonedPageLoadMetricsObserver() = default;

const char* FromGWSAbandonedPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "FromGWSAbandonedPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSAbandonedPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!page_load_metrics::IsGoogleSearchResultUrl(currently_committed_url)) {
    return ObservePolicy::STOP_OBSERVING;
  }

  // Emit a trigger to allow trace collection tied to from gws navigations.
  base::trace_event::EmitNamedTrigger("from-gws-navigation-start");

  category_parameter_id_ =
      page_load_metrics::GetCategoryIdFromUrl(navigation_handle->GetURL());
  impression_ = navigation_handle->GetImpression();
  return AbandonedPageLoadMetricsObserver::OnStart(
      navigation_handle, currently_committed_url, started_in_foreground);
}
page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSAbandonedPageLoadMetricsObserver::OnNavigationHandleTimingUpdated(
    content::NavigationHandle* navigation_handle) {
  auto navigation_handle_timing =
      navigation_handle->GetNavigationHandleTiming();

  if (navigation_handle->GetNetErrorCode() < 0) {
    CHECK(!net_error_.has_value());
    CHECK(!net_extended_error_code_.has_value());
    net_error_ = navigation_handle->GetNetErrorCode();
    net_extended_error_code_ = navigation_handle->GetNetExtendedErrorCode();
  }

  // Set the request / response time of the second redirect by checking:
  // 1) We have not yet recorded second redirect
  // 2) The first request / response has already passed
  // 3) The final non-redirect request / response has not yet passed
  // 4) The most recent request is later than the first request.
  if (second_redirect_request_start_time_.is_null() &&
      second_redirect_response_start_time_.is_null() &&
      !navigation_handle_timing.first_request_start_time.is_null() &&
      !navigation_handle_timing.first_response_start_time.is_null() &&
      navigation_handle_timing.non_redirected_request_start_time.is_null() &&
      navigation_handle_timing.non_redirect_response_start_time.is_null() &&
      navigation_handle_timing.first_request_start_time <
          navigation_handle_timing.final_request_start_time &&
      navigation_handle_timing.first_response_start_time <
          navigation_handle_timing.final_response_start_time) {
    second_redirect_request_start_time_ =
        navigation_handle_timing.final_request_start_time;
    second_redirect_response_start_time_ =
        navigation_handle_timing.final_response_start_time;
  }

  return AbandonedPageLoadMetricsObserver::OnNavigationHandleTimingUpdated(
      navigation_handle);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSAbandonedPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  redirect_num_++;
  return AbandonedPageLoadMetricsObserver::OnRedirect(navigation_handle);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSAbandonedPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  is_committed_ = true;
  return AbandonedPageLoadMetricsObserver::OnCommit(navigation_handle);
}

void FromGWSAbandonedPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(is_committed_);
  AbandonedPageLoadMetricsObserver::OnComplete(timing);
  // We record the metrics here so that we do not forget to record them for
  // all cases. If we did record them for abandonment, and are trying to record
  // the same exact fields, then `LogTimingInformationMetrics()` will avoid
  // recording duplicate entries.
  if (IsAllowedToLogUKM()) {
    LogTimingInformationMetrics();
  }
}

void FromGWSAbandonedPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  CHECK(!is_committed_);
  // Record network error code in case we abort the navigation without going
  // through `OnNavigationHandleTimingUpdated`.
  if (!net_error_.has_value()) {
    net_error_ = failed_provisional_load_info.error;
    net_extended_error_code_ =
        failed_provisional_load_info.net_extended_error_code;
  }
  AbandonedPageLoadMetricsObserver::OnFailedProvisionalLoad(
      failed_provisional_load_info);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FromGWSAbandonedPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // We explicitly record the Timing Information here if we entered app
  // background and have met all the loading milestones,
  // in case we never run `OnComplete()` later.
  if (IsAllowedToLogUKM() && DidLogAllLoadingMilestones()) {
    LogTimingInformationMetrics();
  }
  return AbandonedPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
      timing);
}

std::string FromGWSAbandonedPageLoadMetricsObserver::GetHistogramPrefix()
    const {
  return kFromGWSAbandonedPageLoadMetricsHistogramPrefix;
}

void FromGWSAbandonedPageLoadMetricsObserver::LogTimingInformationMetrics() {
  CHECK(IsAllowedToLogUKM());
  base::TimeTicks now = base::TimeTicks::Now();

  ukm::SourceId source_id =
      ukm::ConvertToSourceId(navigation_id(), ukm::SourceIdType::NAVIGATION_ID);

  ukm::builders::Navigation_FromGoogleSearch_TimingInformation builder(
      source_id);

  auto logged_milestones = LogUKMHistogramsForMilestoneMetrics(builder, now);

  if (!second_redirect_response_start_time_.is_null() &&
      !second_redirect_request_start_time_.is_null()) {
    builder.SetSecondRedirectResponseReceived(true)
        .SetSecondRedirectedRequestStartTime(
            (second_redirect_request_start_time_ - navigation_start_time())
                .InMilliseconds());
    logged_milestones.insert(NavigationMilestone::kSecondRedirectResponseStart);
  } else {
    builder.SetSecondRedirectResponseReceived(false);
  }

  // If we are logging the same exact milestones, then we will omit recording
  // the UKM entry since it would simply be a duplicate.
  if (last_logged_ukm_milestones_.has_value() &&
      last_logged_ukm_milestones_ == logged_milestones) {
    return;
  }

  builder.SetHasImpression(impression_.has_value())
      .SetIsCommitted(is_committed_)
      .SetRedirectCount(redirect_num_);

  if (impression_.has_value()) {
    builder.SetIsEmptyAttributionSrc(impression_->is_empty_attribution_src_tag);
  }

  if (category_parameter_id_.has_value()) {
    builder.SetCategory(category_parameter_id_.value());
  }

  builder.Record(ukm::UkmRecorder::Get());
  last_logged_ukm_milestones_ = logged_milestones;
}

void FromGWSAbandonedPageLoadMetricsObserver::LogUKMHistograms(
    AbandonReason abandon_reason,
    NavigationMilestone milestone,
    base::TimeTicks event_time,
    base::TimeTicks relative_start_time) {
  CHECK(IsAllowedToLogUKM());
  ukm::SourceId source_id =
      ukm::ConvertToSourceId(navigation_id(), ukm::SourceIdType::NAVIGATION_ID);

  ukm::builders::Navigation_FromGoogleSearch_Abandoned builder(source_id);
  // Check if the abandoned milestone is redirect related. If it is, then we
  // check whether a second redirect happened and override the abandoned
  // milestone if possible.
  if (milestone == NavigationMilestone::kFirstRedirectResponseLoaderCallback) {
    if (!second_redirect_response_start_time_.is_null()) {
      milestone = NavigationMilestone::kSecondRedirectResponseStart;
    } else if (!second_redirect_request_start_time_.is_null()) {
      milestone = NavigationMilestone::kSecondRedirectedRequestStart;
    }
  }

  if (net_error_.has_value()) {
    CHECK(net_extended_error_code_.has_value());
    builder.SetNet_ErrorCode(
        std::abs(static_cast<int64_t>(net_error_.value())));
    builder.SetNet_ExtendedErrorCode(
        std::abs(net_extended_error_code_.value()));
  }

  LogUKMHistogramsForAbandonMetrics(builder, abandon_reason, milestone,
                                    event_time, relative_start_time);
  builder.Record(ukm::UkmRecorder::Get());

  // `TimingInformation` ukm is also logged on abandonments.
  LogTimingInformationMetrics();
}

bool FromGWSAbandonedPageLoadMetricsObserver::IsAllowedToLogUKM() const {
  return true;
}

bool FromGWSAbandonedPageLoadMetricsObserver::IsAllowedToLogUMA() const {
  return false;
}

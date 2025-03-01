// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/from_gws_abandoned_page_load_metrics_observer.h"

#include <string>

#include "base/time/time.h"
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
  return AbandonedPageLoadMetricsObserver::OnStart(
      navigation_handle, currently_committed_url, started_in_foreground);
}

std::string FromGWSAbandonedPageLoadMetricsObserver::GetHistogramPrefix()
    const {
  return kFromGWSAbandonedPageLoadMetricsHistogramPrefix;
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
  LogUKMHistogramsForAbandonMetrics(builder, abandon_reason, milestone,
                                    event_time, relative_start_time);
  builder.Record(ukm::UkmRecorder::Get());
}

bool FromGWSAbandonedPageLoadMetricsObserver::IsAllowedToLogUKM() const {
  return true;
}

bool FromGWSAbandonedPageLoadMetricsObserver::IsAllowedToLogUMA() const {
  return false;
}

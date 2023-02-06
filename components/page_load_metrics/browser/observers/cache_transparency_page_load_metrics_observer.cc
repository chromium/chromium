// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/cache_transparency_page_load_metrics_observer.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/features.h"

namespace internal {

const char kHistogramCacheTransparencyFirstContentfulPaint[] =
    "PageLoad.Clients.CacheTransparency.PaintTiming."
    "NavigationToFirstContentfulPaint";
const char kHistogramCacheTransparencyLargestContentfulPaint[] =
    "PageLoad.Clients.CacheTransparency.PaintTiming."
    "NavigationToLargestContentfulPaint2";
const char kHistogramCacheTransparencyInteractionToNextPaint[] =
    "PageLoad.Clients.CacheTransparency.InteractiveTiming."
    "UserInteractionLatency.HighPercentile2.MaxEventDuration";
const char kHistogramCacheTransparencyCumulativeLayoutShift[] =
    "PageLoad.Clients.CacheTransparency.LayoutInstability."
    "MaxCumulativeShiftScore.SessionWindow.Gap1000ms.Max5000ms";

}  // namespace internal

CacheTransparencyPageLoadMetricsObserver::
    CacheTransparencyPageLoadMetricsObserver() = default;
CacheTransparencyPageLoadMetricsObserver::
    ~CacheTransparencyPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CacheTransparencyPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CacheTransparencyPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

void CacheTransparencyPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (logged_ukm_event_) {
    return;
  }
  if (IsCacheTransparencyEnabled()) {
    ukm::builders::PageLoad_CacheTransparencyEnabled(
        GetDelegate().GetPageUkmSourceId())
        .Record(ukm::UkmRecorder::Get());
  } else if (IsPervasivePayloadsEnabled()) {
    ukm::builders::PageLoad_PervasivePayloadsEnabled(
        GetDelegate().GetPageUkmSourceId())
        .Record(ukm::UkmRecorder::Get());
  }
  logged_ukm_event_ = true;
}

void CacheTransparencyPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if ((IsCacheTransparencyEnabled() || IsPervasivePayloadsEnabled()) &&
      page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramCacheTransparencyFirstContentfulPaint,
        timing.paint_timing->first_contentful_paint.value());
  }
}

void CacheTransparencyPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming&) {
  if (IsCacheTransparencyEnabled() || IsPervasivePayloadsEnabled()) {
    RecordTimingHistograms();
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CacheTransparencyPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().DidCommit()) {
    if (IsCacheTransparencyEnabled() || IsPervasivePayloadsEnabled()) {
      RecordTimingHistograms();
    }
  }
  return STOP_OBSERVING;
}

void CacheTransparencyPageLoadMetricsObserver::RecordTimingHistograms() {
  DCHECK(IsCacheTransparencyEnabled() || IsPervasivePayloadsEnabled());
  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (all_frames_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramCacheTransparencyLargestContentfulPaint,
        all_frames_largest_contentful_paint.Time().value());
  }

  const page_load_metrics::NormalizedCLSData& normalized_cls_data =
      GetDelegate().GetNormalizedCLSData(
          page_load_metrics::PageLoadMetricsObserverDelegate::BfcacheStrategy::
              ACCUMULATE);
  if (!normalized_cls_data.data_tainted) {
    page_load_metrics::UmaMaxCumulativeShiftScoreHistogram10000x(
        internal::kHistogramCacheTransparencyCumulativeLayoutShift,
        normalized_cls_data);
  }

  const page_load_metrics::NormalizedResponsivenessMetrics&
      normalized_responsiveness_metrics =
          GetDelegate().GetNormalizedResponsivenessMetrics();
  if (normalized_responsiveness_metrics.num_user_interactions) {
    const page_load_metrics::NormalizedInteractionLatencies&
        max_event_durations =
            normalized_responsiveness_metrics.normalized_max_event_durations;
    base::TimeDelta high_percentile2_max_event_duration = page_load_metrics::
        ResponsivenessMetricsNormalization::ApproximateHighPercentile(
            normalized_responsiveness_metrics.num_user_interactions,
            max_event_durations.worst_ten_latencies);
    base::UmaHistogramCustomTimes(
        internal::kHistogramCacheTransparencyInteractionToNextPaint,
        high_percentile2_max_event_duration, base::Milliseconds(1),
        base::Seconds(60), 50);
  }

  RecordSubresourceLoad();
}

bool CacheTransparencyPageLoadMetricsObserver::IsPervasivePayloadsEnabled() {
  if (!is_pervasive_payloads_enabled_.has_value()) {
    is_pervasive_payloads_enabled_ =
        (base::FeatureList::IsEnabled(
             network::features::kPervasivePayloadsList) &&
         !base::FeatureList::IsEnabled(network::features::kCacheTransparency));
  }
  return is_pervasive_payloads_enabled_.value();
}

bool CacheTransparencyPageLoadMetricsObserver::IsCacheTransparencyEnabled() {
  if (!is_cache_transparency_enabled_.has_value()) {
    is_cache_transparency_enabled_ =
        (base::FeatureList::IsEnabled(network::features::kCacheTransparency) &&
         base::FeatureList::IsEnabled(
             network::features::kPervasivePayloadsList));
  }
  return is_cache_transparency_enabled_.value();
}

void CacheTransparencyPageLoadMetricsObserver::RecordSubresourceLoad() {
  const auto& optional_metrics = GetDelegate().GetSubresourceLoadMetrics();
  if (!optional_metrics) {
    return;
  }
  auto metrics = *optional_metrics;

  ukm::builders::Network_CacheTransparency2(GetDelegate().GetPageUkmSourceId())
      .SetFoundPervasivePayload(metrics.pervasive_payload_requested)
      .SetPervasiveBytesFetched(
          ukm::GetExponentialBucketMinForBytes(metrics.pervasive_bytes_fetched))
      .SetTotalBytesFetched(
          ukm::GetExponentialBucketMinForBytes(metrics.total_bytes_fetched))
      .Record(ukm::UkmRecorder::Get());
}

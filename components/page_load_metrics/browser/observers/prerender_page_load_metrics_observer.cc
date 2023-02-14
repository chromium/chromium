// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/prerender_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace internal {

const char kHistogramPrerenderNavigationToActivation[] =
    "PageLoad.Clients.Prerender.NavigationToActivation";
const char kHistogramPrerenderActivationToFirstPaint[] =
    "PageLoad.Clients.Prerender.PaintTiming.ActivationToFirstPaint";
const char kHistogramPrerenderActivationToFirstContentfulPaint[] =
    "PageLoad.Clients.Prerender.PaintTiming.ActivationToFirstContentfulPaint";
const char kHistogramPrerenderActivationToLargestContentfulPaint2[] =
    "PageLoad.Clients.Prerender.PaintTiming."
    "ActivationToLargestContentfulPaint2";
const char kHistogramPrerenderFirstInputDelay4[] =
    "PageLoad.Clients.Prerender.InteractiveTiming.FirstInputDelay4";
const char kHistogramPrerenderCumulativeShiftScore[] =
    "PageLoad.Clients.Prerender.LayoutInstability.CumulativeShiftScore";
const char kHistogramPrerenderCumulativeShiftScoreMainFrame[] =
    "PageLoad.Clients.Prerender.LayoutInstability.CumulativeShiftScore."
    "MainFrame";
const char
    kHistogramPrerenderMaxCumulativeShiftScoreSessionWindowGap1000msMax5000ms2
        [] = "PageLoad.Clients.Prerender.LayoutInstability."
             "MaxCumulativeShiftScore.SessionWindow."
             "Gap1000ms.Max5000ms2";
const char kHistogramPrerenderPageEndReason[] =
    "PageLoad.Clients.Prerender.PageEndReason";

// Responsiveness metrics.
const char
    kHistogramPrerenderAverageUserInteractionLatencyOverBudgetMaxEventDuration
        [] = "PageLoad.InteractiveTiming."
             "AverageUserInteractionLatencyOverBudget."
             "MaxEventDuration.Prerender";
const char kHistogramPrerenderNumInteractions[] =
    "PageLoad.InteractiveTiming.NumInteractions.Prerender";
const char
    kHistogramPrerenderUserInteractionLatencyHighPercentile2MaxEventDuration[] =
        "PageLoad.InteractiveTiming.UserInteractionLatency."
        "HighPercentile2.MaxEventDuration.Prerender";
const char kHistogramPrerenderWorstUserInteractionLatencyMaxEventDuration[] =
    "PageLoad.InteractiveTiming.WorstUserInteractionLatency.MaxEventDuration."
    "Prerender";

// This metric is used for debugging https://crbug.com/1379491.
// Intentionally this metric doesn't record observer events per trigger type
// (e.g., SpeculationRules) because some functions can be called before
// `PrerenderPageLoadMetricsObserver::trigger_type_` is set (e.g., when
// `OnComplete()` called from the destructor of PageLoadTracker before
// prerender activation).
const char kPageLoadPrerenderObserverEvent[] =
    "PageLoad.Internal.Prerender2.ObserverEvent";

// This metric is used for debugging https://crbug.com/1379491.
const char kPageLoadPrerenderForegroundCheckResult[] =
    "PageLoad.Internal.Prerender2.ForegroundCheckResult";

namespace {

// This is a copy of WasActivatedInForegroundOptionalEventInForeground() in
// page_load_metrics_util.h but with recording diagnose metrics for
// https://crbug.com/1379491. Please keep this consistent with the function.
bool WasActivatedInForegroundOptionalEventInForeground(
    const absl::optional<base::TimeDelta>& event,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate,
    PageLoadPrerenderForegroundCheckEvent event_type) {
  auto result = PageLoadPrerenderForegroundCheckResult::kPassed;
  if (!delegate.WasPrerenderedThenActivatedInForeground()) {
    result = PageLoadPrerenderForegroundCheckResult::kActivatedInBackground;
  } else if (!event) {
    result = PageLoadPrerenderForegroundCheckResult::kNoEventTime;
  } else if (delegate.GetTimeToFirstBackground() &&
             delegate.GetTimeToFirstBackground().value() < event.value()) {
    result = PageLoadPrerenderForegroundCheckResult::kBackgroundedBeforeEvent;
  }

  // Make sure that this function is consistent with the original function.
  CHECK_EQ(result == PageLoadPrerenderForegroundCheckResult::kPassed,
           page_load_metrics::WasActivatedInForegroundOptionalEventInForeground(
               event, delegate));

  std::string histogram_name = kPageLoadPrerenderForegroundCheckResult;
  switch (event_type) {
    case PageLoadPrerenderForegroundCheckEvent::kFirstPaint:
      histogram_name += ".FirstPaint";
      break;
    case PageLoadPrerenderForegroundCheckEvent::kFirstContentfulPaint:
      histogram_name += ".FirstContentfulPaint";
      break;
    case PageLoadPrerenderForegroundCheckEvent::kFirstInputDelay:
      histogram_name += ".FirstInputDelay";
      break;
    case PageLoadPrerenderForegroundCheckEvent::kLargestContentfulPaint:
      histogram_name += ".LargestContentfulPaint";
      break;
  }
  base::UmaHistogramEnumeration(histogram_name, result);

  return result == PageLoadPrerenderForegroundCheckResult::kPassed;
}

}  // namespace

}  // namespace internal

PrerenderPageLoadMetricsObserver::PrerenderPageLoadMetricsObserver() = default;
PrerenderPageLoadMetricsObserver::~PrerenderPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrerenderPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrerenderPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(https://crbug.com/1335481): Prerendering pages embedding FencedFrames
  // are not supported. So, this class doesn't need forwarding.
  DCHECK(!navigation_handle->IsInPrerenderedMainFrame());
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrerenderPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  base::UmaHistogramEnumeration(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnPrerenderStart);

  // TODO(https://crbug.com/1335481): Prerendering pages embedding FencedFrames
  // are not supported.
  DCHECK(navigation_handle->GetNavigatingFrameType() !=
         content::FrameType::kFencedFrameRoot);
  return CONTINUE_OBSERVING;
}

void PrerenderPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  base::UmaHistogramEnumeration(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kDidActivatePrerenderedPage);

  // Copy the trigger type and histogram suffix for an embedder. These data will
  // be lost after NavigationRequest is destroyed.
  DCHECK(!trigger_type_.has_value());
  trigger_type_ = navigation_handle->GetPrerenderTriggerType();
  embedder_histogram_suffix_ =
      navigation_handle->GetPrerenderEmbedderHistogramSuffix();

  const net::HttpResponseHeaders* response_headers =
      navigation_handle->GetResponseHeaders();
  if (response_headers) {
    main_frame_resource_has_no_store_ =
        response_headers->HasHeaderValue("cache-control", "no-store");
  }

  // |navigation_handle| here is for the activation navigation, while
  // |GetDelegate().GetNavigationStart()| is the start time of initial prerender
  // navigation.
  base::TimeDelta navigation_to_activation =
      navigation_handle->NavigationStart() - GetDelegate().GetNavigationStart();
  base::UmaHistogramCustomTimes(
      AppendSuffix(internal::kHistogramPrerenderNavigationToActivation),
      navigation_to_activation, base::Milliseconds(10), base::Minutes(10), 100);

  ukm::builders::PrerenderPageLoad builder(GetDelegate().GetPageUkmSourceId());
  if (main_frame_resource_has_no_store_.has_value()) {
    builder.SetMainFrameResource_RequestHasNoStore(
        main_frame_resource_has_no_store_.value() ? 1 : 0);
  }

  builder.SetWasPrerendered(true).SetTiming_NavigationToActivation(
      navigation_to_activation.InMilliseconds());
  builder.Record(ukm::UkmRecorder::Get());
}

void PrerenderPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::UmaHistogramEnumeration(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnFirstPaintInPage);

  if (!internal::WasActivatedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate(),
          internal::PageLoadPrerenderForegroundCheckEvent::kFirstPaint)) {
    return;
  }
  base::UmaHistogramCustomTimes(
      AppendSuffix(internal::kHistogramPrerenderActivationToFirstPaint),
      timing.paint_timing->first_paint.value() -
          timing.activation_start.value(),
      base::Milliseconds(10), base::Minutes(10), 100);
}

void PrerenderPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::UmaHistogramEnumeration(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnFirstContentfulPaintInPage);

  if (!internal::WasActivatedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate(),
          internal::PageLoadPrerenderForegroundCheckEvent::
              kFirstContentfulPaint)) {
    return;
  }
  base::TimeDelta activation_to_fcp =
      timing.paint_timing->first_contentful_paint.value() -
      timing.activation_start.value();
  base::UmaHistogramCustomTimes(
      AppendSuffix(
          internal::kHistogramPrerenderActivationToFirstContentfulPaint),
      activation_to_fcp, base::Milliseconds(10), base::Minutes(10), 100);
  ukm::builders::PrerenderPageLoad(GetDelegate().GetPageUkmSourceId())
      .SetTiming_ActivationToFirstContentfulPaint(
          activation_to_fcp.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void PrerenderPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::UmaHistogramEnumeration(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnFirstInputInPage);

  if (!internal::WasActivatedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate(),
          internal::PageLoadPrerenderForegroundCheckEvent::kFirstInputDelay)) {
    return;
  }

  base::TimeDelta first_input_delay =
      timing.interactive_timing->first_input_delay.value();
  base::UmaHistogramCustomTimes(
      AppendSuffix(internal::kHistogramPrerenderFirstInputDelay4),
      first_input_delay, base::Milliseconds(1), base::Seconds(60), 50);
  ukm::builders::PrerenderPageLoad(GetDelegate().GetPageUkmSourceId())
      .SetInteractiveTiming_FirstInputDelay4(first_input_delay.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void PrerenderPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::UmaHistogramEnumeration(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kOnComplete);
  RecordSessionEndHistograms(timing, /*app_entering_background=*/false);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrerenderPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::UmaHistogramEnumeration(internal::kPageLoadPrerenderObserverEvent,
                                internal::PageLoadPrerenderObserverEvent::
                                    kFlushMetricsOnAppEnterBackground);
  RecordSessionEndHistograms(timing, /*app_entering_background=*/true);
  return STOP_OBSERVING;
}

void PrerenderPageLoadMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing,
    bool app_entering_background) {
  base::UmaHistogramEnumeration(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kRecordSessionEndHistograms);

  if (!GetDelegate().WasPrerenderedThenActivatedInForeground() ||
      !main_frame_timing.activation_start) {
    // Even if the page was activated, activation_start may not yet been
    // notified by the renderer. Ignore such page loads.
    return;
  }

  // Records the reason how a page load ends.
  auto page_end_reason = GetDelegate().GetPageEndReason();
  if (page_end_reason == page_load_metrics::PageEndReason::END_NONE &&
      app_entering_background) {
    page_end_reason =
        page_load_metrics::PageEndReason::END_APP_ENTER_BACKGROUND;
  }
  ukm::builders::PrerenderPageLoad(GetDelegate().GetPageUkmSourceId())
      .SetPageEndReason(page_end_reason)
      .Record(ukm::UkmRecorder::Get());
  base::UmaHistogramEnumeration(
      AppendSuffix(internal::kHistogramPrerenderPageEndReason), page_end_reason,
      page_load_metrics::PAGE_END_REASON_COUNT);

  // Records Largest Contentful Paint (LCP) to UMA and UKM.
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      internal::WasActivatedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate(),
          internal::PageLoadPrerenderForegroundCheckEvent::
              kLargestContentfulPaint)) {
    base::TimeDelta activation_to_lcp =
        largest_contentful_paint.Time().value() -
        main_frame_timing.activation_start.value();
    base::UmaHistogramCustomTimes(
        AppendSuffix(
            internal::kHistogramPrerenderActivationToLargestContentfulPaint2),
        activation_to_lcp, base::Milliseconds(10), base::Minutes(10), 100);
    ukm::builders::PrerenderPageLoad(GetDelegate().GetPageUkmSourceId())
        .SetTiming_ActivationToLargestContentfulPaint(
            activation_to_lcp.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }

  // Record metrics only when a prerendered page is successfully activated.
  // TODO(crbug.com/1364013): add tests to make sure that CLS and INP metrics
  // are not recorded when prerendering is canceled.
  if (GetDelegate().GetPrerenderingState() ==
      page_load_metrics::PrerenderingState::kActivated) {
    RecordLayoutShiftScoreMetrics(main_frame_timing);
    RecordNormalizedResponsivenessMetrics();
  }
}

void PrerenderPageLoadMetricsObserver::RecordLayoutShiftScoreMetrics(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  base::UmaHistogramEnumeration(
      internal::kPageLoadPrerenderObserverEvent,
      internal::PageLoadPrerenderObserverEvent::kRecordLayoutShiftScoreMetrics);

  DCHECK(GetDelegate().WasPrerenderedThenActivatedInForeground());
  DCHECK(main_frame_timing.activation_start);

  base::UmaHistogramCounts100(
      AppendSuffix(internal::kHistogramPrerenderCumulativeShiftScore),
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetPageRenderData().layout_shift_score));
  base::UmaHistogramCounts100(
      AppendSuffix(internal::kHistogramPrerenderCumulativeShiftScoreMainFrame),
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetMainFrameRenderData().layout_shift_score));

  const page_load_metrics::NormalizedCLSData& normalized_cls_data =
      GetDelegate().GetNormalizedCLSData(
          page_load_metrics::PageLoadMetricsObserverDelegate::BfcacheStrategy::
              ACCUMULATE);
  if (normalized_cls_data.data_tainted)
    return;

  page_load_metrics::UmaMaxCumulativeShiftScoreHistogram10000x(
      AppendSuffix(
          internal::
              kHistogramPrerenderMaxCumulativeShiftScoreSessionWindowGap1000msMax5000ms2),
      normalized_cls_data);
  const float max_cls =
      normalized_cls_data.session_windows_gap1000ms_max5000ms_max_cls;
  ukm::builders::PrerenderPageLoad(GetDelegate().GetPageUkmSourceId())
      .SetLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000ms(
          page_load_metrics::LayoutShiftUkmValue(max_cls))
      .Record(ukm::UkmRecorder::Get());
}

void PrerenderPageLoadMetricsObserver::RecordNormalizedResponsivenessMetrics() {
  base::UmaHistogramEnumeration(internal::kPageLoadPrerenderObserverEvent,
                                internal::PageLoadPrerenderObserverEvent::
                                    kRecordNormalizedResponsivenessMetrics);

  DCHECK(GetDelegate().WasPrerenderedThenActivatedInForeground());

  const page_load_metrics::NormalizedResponsivenessMetrics&
      normalized_responsiveness_metrics =
          GetDelegate().GetNormalizedResponsivenessMetrics();
  if (!normalized_responsiveness_metrics.num_user_interactions)
    return;

  const page_load_metrics::NormalizedInteractionLatencies& max_event_durations =
      normalized_responsiveness_metrics.normalized_max_event_durations;

  base::TimeDelta high_percentile2_max_event_duration = page_load_metrics::
      ResponsivenessMetricsNormalization::ApproximateHighPercentile(
          normalized_responsiveness_metrics.num_user_interactions,
          max_event_durations.worst_ten_latencies);

  UmaHistogramCustomTimes(
      internal::kHistogramPrerenderWorstUserInteractionLatencyMaxEventDuration,
      max_event_durations.worst_latency, base::Milliseconds(1),
      base::Seconds(60), 50);
  UmaHistogramCustomTimes(
      internal::
          kHistogramPrerenderAverageUserInteractionLatencyOverBudgetMaxEventDuration,
      max_event_durations.sum_of_latency_over_budget /
          normalized_responsiveness_metrics.num_user_interactions,
      base::Milliseconds(1), base::Seconds(60), 50);
  UmaHistogramCustomTimes(
      internal::
          kHistogramPrerenderUserInteractionLatencyHighPercentile2MaxEventDuration,
      high_percentile2_max_event_duration, base::Milliseconds(1),
      base::Seconds(60), 50);
  base::UmaHistogramCounts1000(
      internal::kHistogramPrerenderNumInteractions,
      normalized_responsiveness_metrics.num_user_interactions);

  ukm::builders::PrerenderPageLoad builder(GetDelegate().GetPageUkmSourceId());
  builder.SetInteractiveTiming_WorstUserInteractionLatency_MaxEventDuration(
      max_event_durations.worst_latency.InMilliseconds());
  builder
      .SetInteractiveTiming_AverageUserInteractionLatencyOverBudget_MaxEventDuration(
          max_event_durations.sum_of_latency_over_budget.InMilliseconds() /
          normalized_responsiveness_metrics.num_user_interactions);

  builder
      .SetInteractiveTiming_UserInteractionLatency_HighPercentile2_MaxEventDuration(
          high_percentile2_max_event_duration.InMilliseconds());
  builder.SetInteractiveTiming_NumInteractions(
      ukm::GetExponentialBucketMinForCounts1000(
          normalized_responsiveness_metrics.num_user_interactions));

  builder.Record(ukm::UkmRecorder::Get());
}

std::string PrerenderPageLoadMetricsObserver::AppendSuffix(
    const std::string& histogram_name) const {
  DCHECK(trigger_type_.has_value());
  switch (trigger_type_.value()) {
    case content::PrerenderTriggerType::kSpeculationRule:
      DCHECK(embedder_histogram_suffix_.empty());
      return histogram_name + ".SpeculationRule";
    case content::PrerenderTriggerType::kEmbedder:
      DCHECK(!embedder_histogram_suffix_.empty());
      return histogram_name + ".Embedder_" + embedder_histogram_suffix_;
  }
  NOTREACHED();
}

// Copyright 2021 The Chromium Authors. All rights reserved.
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
  // TODO(https://crbug.com/1335481): Prerendering pages embedding FencedFrames
  // are not supported.
  DCHECK(navigation_handle->GetNavigatingFrameType() !=
         content::FrameType::kFencedFrameRoot);
  return CONTINUE_OBSERVING;
}

void PrerenderPageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  // Copy the trigger type and histogram suffix for an embedder. These data will
  // be lost after NavigationRequest is destroyed.
  DCHECK(!trigger_type_.has_value());
  trigger_type_ = navigation_handle->GetPrerenderTriggerType();
  embedder_histogram_suffix_ =
      navigation_handle->GetPrerenderEmbedderHistogramSuffix();

  // |navigation_handle| here is for the activation navigation, while
  // |GetDelegate().GetNavigationStart()| is the start time of initial prerender
  // navigation.
  base::TimeDelta navigation_to_activation =
      navigation_handle->NavigationStart() - GetDelegate().GetNavigationStart();
  base::UmaHistogramCustomTimes(
      AppendSuffix(internal::kHistogramPrerenderNavigationToActivation),
      navigation_to_activation, base::Milliseconds(10), base::Minutes(10), 100);

  ukm::builders::PrerenderPageLoad(GetDelegate().GetPageUkmSourceId())
      .SetWasPrerendered(true)
      .SetTiming_NavigationToActivation(
          navigation_to_activation.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void PrerenderPageLoadMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!WasActivatedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
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
  if (!WasActivatedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
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
  if (!WasActivatedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
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
  RecordSessionEndHistograms(timing);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrerenderPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
  return STOP_OBSERVING;
}

void PrerenderPageLoadMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  if (!GetDelegate().WasPrerenderedThenActivatedInForeground() ||
      !main_frame_timing.activation_start) {
    // Even if the page was activated, activation_start may not yet been
    // notified by the renderer. Ignore such page loads.
    return;
  }

  // Records Largest Contentful Paint (LCP) to UMA and UKM.
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      WasActivatedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
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

  // Records Cumulative Layout Shift Score (CLS) to UMA and UKM.
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
  if (!normalized_cls_data.data_tainted) {
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

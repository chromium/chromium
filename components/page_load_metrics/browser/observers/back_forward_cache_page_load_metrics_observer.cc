// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/back_forward_cache_page_load_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/observers/core/uma_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/responsiveness_metrics_normalization.h"
#include "components/page_load_metrics/common/page_visit_final_status.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"

using page_load_metrics::PageVisitFinalStatus;

namespace internal {

const char kHistogramFirstPaintAfterBackForwardCacheRestore[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint.AfterBackForwardCacheRestore";
const char kHistogramFirstRequestAnimationFrameAfterBackForwardCacheRestore[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint.BFCachePolyfillFirst";
const char kHistogramSecondRequestAnimationFrameAfterBackForwardCacheRestore[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint.BFCachePolyfillSecond";
const char kHistogramThirdRequestAnimationFrameAfterBackForwardCacheRestore[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint.BFCachePolyfillThird";
const char kHistogramFirstInputDelayAfterBackForwardCacheRestore[] =
    "PageLoad.InteractiveTiming.FirstInputDelay.AfterBackForwardCacheRestore";
extern const char
    kHistogramCumulativeShiftScoreMainFrameAfterBackForwardCacheRestore[] =
        "PageLoad.LayoutInstability.CumulativeShiftScore.MainFrame."
        "AfterBackForwardCacheRestore";
extern const char kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore[] =
    "PageLoad.LayoutInstability.CumulativeShiftScore."
    "AfterBackForwardCacheRestore";

const char kNumInteractions_AfterBackForwardCacheRestore[] =
    "PageLoad.InteractiveTiming.NumInteractions.AfterBackForwardCacheRestore";
const char
    kUserInteractionLatencyHighPercentile2_MaxEventDuration_AfterBackForwardCacheRestore
        [] = "PageLoad.InteractiveTiming."
             "UserInteractionLatency."
             "HighPercentile2.MaxEventDuration.AfterBackForwardCacheRestore";
const char
    kWorstUserInteractionLatency_MaxEventDuration_AfterBackForwardCacheRestore
        [] = "PageLoad.InteractiveTiming."
             "WorstUserInteractionLatency."
             "MaxEventDuration.AfterBackForwardCacheRestore";

// Enables to emit zero values for some key metrics when back-forward cache is
// used.
//
// With this flag disabled, no samples are emitted for regular VOLT metrics
// after the page is restored from the back-forward cache. This means that we
// will miss a lot of metrics for history navigations after we launch back-
// forward cache. As metrics for history navigations tend to be better figures
// than other navigations (e.g., due to network cache), the average of such
// metrics values will become worse and might seem regression if we don't take
// any actions.
//
// To mitigate this issue, we plan to emit 0 samples for such key metrics for
// back-forward navigations. This is implemented behind this flag so far, and we
// will enable this by default when we reach the conclusion how to adjust them.
//
// For cumulative layout shift scores, we use actual score values for back-
// forward cache navigations instead of 0s.
BASE_FEATURE(kBackForwardCacheEmitZeroSamplesForKeyMetrics,
             "BackForwardCacheEmitZeroSamplesForKeyMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace internal

BackForwardCachePageLoadMetricsObserver::
    BackForwardCachePageLoadMetricsObserver() = default;

BackForwardCachePageLoadMetricsObserver::
    ~BackForwardCachePageLoadMetricsObserver() {
  // TODO(crbug.com/40203717): Revert to the default destructor when we've
  // figured out why sometimes page end metrics are not logged.
  if (back_forward_cache_navigation_ids_.size() > 0) {
    DCHECK(logged_page_end_metrics_);
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
BackForwardCachePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  was_hidden_ = !started_in_foreground;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
BackForwardCachePageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(crbug.com/40198346): This must be updated when FencedFrames
  // supports back/forward cache.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
BackForwardCachePageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class mainly interested in the behavior after entreing Back/Forward
  // Cache. Works as same as non prerendering case.
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
BackForwardCachePageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!in_back_forward_cache_) {
    MaybeRecordForegroundDurationAfterBackForwardCacheRestore(
        base::DefaultTickClock::GetInstance(),
        /*app_entering_background=*/false);
  }
  was_hidden_ = true;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
BackForwardCachePageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  in_back_forward_cache_ = true;
  RecordMetricsOnPageVisitEnd(timing, /*app_entering_background=*/false);
  has_ever_entered_back_forward_cache_ = true;
  return CONTINUE_OBSERVING;
}

void BackForwardCachePageLoadMetricsObserver::OnRestoreFromBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    content::NavigationHandle* navigation_handle) {
  page_metrics_logged_due_to_backgrounding_ = false;
  in_back_forward_cache_ = false;
  back_forward_cache_navigation_ids_.push_back(
      navigation_handle->GetNavigationId());
  content::WebContents* web_contents = GetDelegate().GetWebContents();
  was_hidden_ = web_contents &&
                web_contents->GetVisibility() == content::Visibility::HIDDEN;
  logged_page_end_metrics_ = false;
  restored_main_frame_layout_shift_score_ =
      GetDelegate().GetMainFrameRenderData().layout_shift_score;
  restored_layout_shift_score_ =
      GetDelegate().GetPageRenderData().layout_shift_score;
  // HistoryNavigation is a singular event, and we share the same instance as
  // long as we use the same source ID.
  ukm::builders::HistoryNavigation builder(
      GetUkmSourceIdForBackForwardCacheRestore(
          back_forward_cache_navigation_ids_.size() - 1));
  bool amp_flag = GetDelegate().GetMainFrameMetadata().behavior_flags &
                  blink::kLoadingBehaviorAmpDocumentLoaded;
  builder.SetBackForwardCache_IsAmpPage(amp_flag);
  builder.Record(ukm::UkmRecorder::Get());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
BackForwardCachePageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  PageLoadMetricsObserver::ObservePolicy policy =
      PageLoadMetricsObserver::ShouldObserveMimeType(mime_type);
  if (policy == STOP_OBSERVING && has_ever_entered_back_forward_cache_) {
    // We should not record UKMs while prerendering. But the page in
    // prerendering is not eligible for Back/Forward Cache and
    // `has_ever_entered_back_forward_cache_` implies the page is not in
    // prerendering. So, we can record UKM safely.
    DCHECK_NE(GetDelegate().GetPrerenderingState(),
              page_load_metrics::PrerenderingState::kInPrerendering);

    ukm::builders::UserPerceivedPageVisit(
        GetLastUkmSourceIdForBackForwardCacheRestore())
        .SetNotCountedForCoreWebVitals(true)
        .Record(ukm::UkmRecorder::Get());
  }
  return policy;
}

void BackForwardCachePageLoadMetricsObserver::
    OnFirstPaintAfterBackForwardCacheRestoreInPage(
        const page_load_metrics::mojom::BackForwardCacheTiming& timing,
        size_t index) {
  if (index >= back_forward_cache_navigation_ids_.size())
    return;
  auto first_paint = timing.first_paint_after_back_forward_cache_restore;
  DCHECK(!first_paint.is_zero());
  if (page_load_metrics::
          WasStartedInForegroundOptionalEventInForegroundAfterBackForwardCacheRestore(
              first_paint, GetDelegate(), index)) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramFirstPaintAfterBackForwardCacheRestore,
        first_paint);

    // HistoryNavigation is a singular event, and we share the same instance as
    // long as we use the same source ID.
    ukm::builders::HistoryNavigation builder(
        GetUkmSourceIdForBackForwardCacheRestore(index));
    builder.SetNavigationToFirstPaintAfterBackForwardCacheRestore(
        first_paint.InMilliseconds());
    builder.Record(ukm::UkmRecorder::Get());

    if (base::FeatureList::IsEnabled(
            internal::kBackForwardCacheEmitZeroSamplesForKeyMetrics)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstPaint, base::TimeDelta{});
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaint,
                          base::TimeDelta{});
      PAGE_LOAD_HISTOGRAM(internal::kHistogramLargestContentfulPaint,
                          base::TimeDelta{});
    }
  }
}

void BackForwardCachePageLoadMetricsObserver::
    OnRequestAnimationFramesAfterBackForwardCacheRestoreInPage(
        const page_load_metrics::mojom::BackForwardCacheTiming& timing,
        size_t index) {
  if (index >= back_forward_cache_navigation_ids_.size())
    return;
  auto request_animation_frames =
      timing.request_animation_frames_after_back_forward_cache_restore;
  DCHECK_EQ(request_animation_frames.size(), 3u);

  PAGE_LOAD_HISTOGRAM(
      internal::
          kHistogramFirstRequestAnimationFrameAfterBackForwardCacheRestore,
      request_animation_frames[0]);
  PAGE_LOAD_HISTOGRAM(
      internal::
          kHistogramSecondRequestAnimationFrameAfterBackForwardCacheRestore,
      request_animation_frames[1]);
  PAGE_LOAD_HISTOGRAM(
      internal::
          kHistogramThirdRequestAnimationFrameAfterBackForwardCacheRestore,
      request_animation_frames[2]);

  // HistoryNavigation is a singular event, and we share the same instance as
  // long as we use the same source ID.
  ukm::builders::HistoryNavigation builder(
      GetUkmSourceIdForBackForwardCacheRestore(index));
  builder.SetFirstRequestAnimationFrameAfterBackForwardCacheRestore(
      request_animation_frames[0].InMilliseconds());
  builder.SetSecondRequestAnimationFrameAfterBackForwardCacheRestore(
      request_animation_frames[1].InMilliseconds());
  builder.SetThirdRequestAnimationFrameAfterBackForwardCacheRestore(
      request_animation_frames[2].InMilliseconds());
  builder.Record(ukm::UkmRecorder::Get());
}

void BackForwardCachePageLoadMetricsObserver::
    OnFirstInputAfterBackForwardCacheRestoreInPage(
        const page_load_metrics::mojom::BackForwardCacheTiming& timing,
        size_t index) {
  if (index >= back_forward_cache_navigation_ids_.size())
    return;
  auto first_input_delay =
      timing.first_input_delay_after_back_forward_cache_restore;
  DCHECK(first_input_delay.has_value());
  if (page_load_metrics::
          WasStartedInForegroundOptionalEventInForegroundAfterBackForwardCacheRestore(
              first_input_delay, GetDelegate(), index)) {
    base::UmaHistogramCustomTimes(
        internal::kHistogramFirstInputDelayAfterBackForwardCacheRestore,
        *first_input_delay, base::Milliseconds(1), base::Seconds(60), 50);

    // HistoryNavigation is a singular event, and we share the same instance as
    // long as we use the same source ID.
    ukm::builders::HistoryNavigation builder(
        GetUkmSourceIdForBackForwardCacheRestore(index));
    builder.SetFirstInputDelayAfterBackForwardCacheRestore(
        first_input_delay.value().InMilliseconds());
    builder.Record(ukm::UkmRecorder::Get());

    if (base::FeatureList::IsEnabled(
            internal::kBackForwardCacheEmitZeroSamplesForKeyMetrics)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstInputDelay,
                          base::TimeDelta{});
    }
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
BackForwardCachePageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!in_back_forward_cache_)
    RecordMetricsOnPageVisitEnd(timing, /*app_entering_background=*/true);
  page_metrics_logged_due_to_backgrounding_ = true;
  return CONTINUE_OBSERVING;
}

void BackForwardCachePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // If the page is in the back-forward cache and OnComplete is called, the page
  // is being evicted from the cache. Do not record metrics here as we have
  // already recorded them in OnEnterBackForwardCache.
  if (in_back_forward_cache_)
    return;
  RecordMetricsOnPageVisitEnd(timing, /*app_entering_background=*/false);
}

void BackForwardCachePageLoadMetricsObserver::RecordMetricsOnPageVisitEnd(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    bool app_entering_background) {
  if (page_metrics_logged_due_to_backgrounding_)
    return;
  MaybeRecordLayoutShiftScoreAfterBackForwardCacheRestore(timing);
  MaybeRecordPageEndAfterBackForwardCacheRestore(app_entering_background);
  MaybeRecordForegroundDurationAfterBackForwardCacheRestore(
      base::DefaultTickClock::GetInstance(), app_entering_background);
  MaybeRecordNormalizedResponsivenessMetrics();

  if (has_ever_entered_back_forward_cache_) {
    page_load_metrics::RecordPageVisitFinalStatusForTiming(
        timing, GetDelegate(), GetLastUkmSourceIdForBackForwardCacheRestore());
    bool is_user_initiated_navigation =
        // All browser initiated page loads are user-initiated.
        GetDelegate().GetUserInitiatedInfo().browser_initiated ||
        // Renderer-initiated navigations are user-initiated if there is an
        // associated input event.
        GetDelegate().GetUserInitiatedInfo().user_input_event;
    ukm::builders::UserPerceivedPageVisit(
        GetLastUkmSourceIdForBackForwardCacheRestore())
        .SetUserInitiated(is_user_initiated_navigation)
        .Record(ukm::UkmRecorder::Get());
  }
}

void BackForwardCachePageLoadMetricsObserver::
    MaybeRecordNormalizedResponsivenessMetrics() {
  if (!has_ever_entered_back_forward_cache_)
    return;
  // Normalized Responsiveness Metrics.
  const page_load_metrics::ResponsivenessMetricsNormalization&
      responsiveness_metrics_normalization =
          GetDelegate().GetResponsivenessMetricsNormalization();

  if (!responsiveness_metrics_normalization.num_user_interactions()) {
    return;
  }

  // HistoryNavigation is a singular event, and we share the same instance as
  // long as we use the same source ID.
  ukm::builders::HistoryNavigation builder(
      GetLastUkmSourceIdForBackForwardCacheRestore());
  builder
      .SetWorstUserInteractionLatencyAfterBackForwardCacheRestore_MaxEventDuration2(
          responsiveness_metrics_normalization.worst_latency()
              .value()
              .interaction_latency.InMilliseconds());
  UmaHistogramCustomTimes(
      internal::
          kWorstUserInteractionLatency_MaxEventDuration_AfterBackForwardCacheRestore,
      responsiveness_metrics_normalization.worst_latency()
          .value()
          .interaction_latency,
      base::Milliseconds(1), base::Seconds(60), 50);

  base::TimeDelta high_percentile2_max_event_duration =
      responsiveness_metrics_normalization.ApproximateHighPercentile()
          .value()
          .interaction_latency;
  builder
      .SetUserInteractionLatencyAfterBackForwardCacheRestore_HighPercentile2_MaxEventDuration(
          high_percentile2_max_event_duration.InMilliseconds());
  builder.SetNumInteractionsAfterBackForwardCacheRestore(
      ukm::GetExponentialBucketMinForCounts1000(
          responsiveness_metrics_normalization.num_user_interactions()));

  UmaHistogramCustomTimes(
      internal::
          kUserInteractionLatencyHighPercentile2_MaxEventDuration_AfterBackForwardCacheRestore,
      high_percentile2_max_event_duration, base::Milliseconds(1),
      base::Seconds(60), 50);
  base::UmaHistogramCounts1000(
      internal::kNumInteractions_AfterBackForwardCacheRestore,
      responsiveness_metrics_normalization.num_user_interactions());

  builder.Record(ukm::UkmRecorder::Get());
}

void BackForwardCachePageLoadMetricsObserver::
    MaybeRecordLayoutShiftScoreAfterBackForwardCacheRestore(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!has_ever_entered_back_forward_cache_ ||
      !restored_main_frame_layout_shift_score_.has_value()) {
    return;
  }
  DCHECK(restored_layout_shift_score_.has_value());
  double layout_main_frame_shift_score =
      GetDelegate().GetMainFrameRenderData().layout_shift_score -
      restored_main_frame_layout_shift_score_.value();
  DCHECK_GE(layout_main_frame_shift_score, 0);
  double layout_shift_score =
      GetDelegate().GetPageRenderData().layout_shift_score -
      restored_layout_shift_score_.value();
  DCHECK_GE(layout_shift_score, 0);

  base::UmaHistogramCounts100(
      internal::
          kHistogramCumulativeShiftScoreMainFrameAfterBackForwardCacheRestore,
      page_load_metrics::LayoutShiftUmaValue(layout_main_frame_shift_score));
  base::UmaHistogramCounts100(
      internal::kHistogramCumulativeShiftScoreAfterBackForwardCacheRestore,
      page_load_metrics::LayoutShiftUmaValue(layout_shift_score));

  // HistoryNavigation is a singular event, and we share the same instance as
  // long as we use the same source ID.
  ukm::builders::HistoryNavigation builder(
      GetLastUkmSourceIdForBackForwardCacheRestore());
  builder.SetCumulativeShiftScoreAfterBackForwardCacheRestore(
      page_load_metrics::LayoutShiftUkmValue(layout_shift_score));
  page_load_metrics::NormalizedCLSData normalized_cls_data =
      GetDelegate().GetNormalizedCLSData(
          page_load_metrics::PageLoadMetricsObserverDelegate::BfcacheStrategy::
              RESET);
  if (!normalized_cls_data.data_tainted) {
    builder
        .SetMaxCumulativeShiftScoreAfterBackForwardCacheRestore_SessionWindow_Gap1000ms_Max5000ms(
            page_load_metrics::LayoutShiftUkmValue(
                normalized_cls_data
                    .session_windows_gap1000ms_max5000ms_max_cls));
    base::UmaHistogramCustomCounts(
        "PageLoad.LayoutInstability.MaxCumulativeShiftScore."
        "AfterBackForwardCacheRestore.SessionWindow.Gap1000ms.Max5000ms2",
        page_load_metrics::LayoutShiftUmaValue10000(
            normalized_cls_data.session_windows_gap1000ms_max5000ms_max_cls),
        1, 24000, 50);
  }

  builder.Record(ukm::UkmRecorder::Get());

  if (base::FeatureList::IsEnabled(
          internal::kBackForwardCacheEmitZeroSamplesForKeyMetrics)) {
    base::UmaHistogramCounts100(
        "PageLoad.LayoutInstability.CumulativeShiftScore.MainFrame",
        page_load_metrics::LayoutShiftUmaValue(layout_main_frame_shift_score));
    base::UmaHistogramCounts100(
        "PageLoad.LayoutInstability.CumulativeShiftScore",
        page_load_metrics::LayoutShiftUmaValue(layout_shift_score));
  }
}

void BackForwardCachePageLoadMetricsObserver::
    MaybeRecordPageEndAfterBackForwardCacheRestore(
        bool app_entering_background) {
  if (!has_ever_entered_back_forward_cache_)
    return;
  auto page_end_reason = GetDelegate().GetPageEndReason();
  if (page_end_reason == page_load_metrics::PageEndReason::END_NONE &&
      app_entering_background) {
    page_end_reason =
        page_load_metrics::PageEndReason::END_APP_ENTER_BACKGROUND;
  }
  // HistoryNavigation is a singular event, and we share the same instance as
  // long as we use the same source ID.
  ukm::builders::HistoryNavigation builder(
      GetLastUkmSourceIdForBackForwardCacheRestore());
  builder.SetPageEndReasonAfterBackForwardCacheRestore(page_end_reason);
  builder.Record(ukm::UkmRecorder::Get());
  logged_page_end_metrics_ = true;
}

void BackForwardCachePageLoadMetricsObserver::
    MaybeRecordForegroundDurationAfterBackForwardCacheRestore(
        const base::TickClock* clock,
        bool app_entering_background) const {
  if (!was_hidden_ && has_ever_entered_back_forward_cache_) {
    // This logic for finding the foreground duration is intended to mimic
    // page_load_metrics::GetInitialForegroundDuration, but adjusted to
    // take into account the back forward cache.
    std::optional<base::TimeDelta> foreground_duration;
    DCHECK(back_forward_cache_navigation_ids_.size() >= 1);
    auto back_forward_state = GetDelegate().GetBackForwardCacheRestore(
        back_forward_cache_navigation_ids_.size() - 1);

    // If the BFCache restoration happened while not in the foreground, don't
    // record a foreground duration.
    if (!back_forward_state.was_in_foreground)
      return;

    std::optional<base::TimeDelta> time_to_page_end =
        GetDelegate().GetPageEndReason() == page_load_metrics::END_NONE
            ? std::optional<base::TimeDelta>()
            : GetDelegate().GetPageEndTime() -
                  back_forward_state.navigation_start_time;

    // |first_background_time| is actually time-to-first-background here, i.e.
    // it's a delta, not an absolute time, so does not need to be adjusted by
    // navigation start time.
    foreground_duration = page_load_metrics::OptionalMin(
        back_forward_state.first_background_time, time_to_page_end);

    if (!foreground_duration && app_entering_background) {
      foreground_duration =
          clock->NowTicks() - back_forward_state.navigation_start_time;
    }

    if (foreground_duration.has_value()) {
      ukm::builders::HistoryNavigation builder(
          GetLastUkmSourceIdForBackForwardCacheRestore());
      builder.SetForegroundDurationAfterBackForwardCacheRestore(
          ukm::GetSemanticBucketMinForDurationTiming(
              foreground_duration.value().InMilliseconds()));
      builder.Record(ukm::UkmRecorder::Get());
    }
  }
}

int64_t BackForwardCachePageLoadMetricsObserver::
    GetUkmSourceIdForBackForwardCacheRestore(size_t index) const {
  DCHECK_GT(back_forward_cache_navigation_ids_.size(), index);
  int64_t navigation_id = back_forward_cache_navigation_ids_[index];
  DCHECK_NE(ukm::kInvalidSourceId, navigation_id);
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

int64_t BackForwardCachePageLoadMetricsObserver::
    GetLastUkmSourceIdForBackForwardCacheRestore() const {
  int64_t navigation_id = back_forward_cache_navigation_ids_.back();
  DCHECK_NE(ukm::kInvalidSourceId, navigation_id);
  return ukm::ConvertToSourceId(navigation_id,
                                ukm::SourceIdType::NAVIGATION_ID);
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/page_stability_metrics.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/common/actor/page_stability_metrics_common.h"
#include "chrome/renderer/actor/page_stability_monitor.h"

namespace actor {

PageStabilityMetrics::PageStabilityMetrics() = default;

PageStabilityMetrics::~PageStabilityMetrics() {
  Flush();
}

void PageStabilityMetrics::Start() {
  CHECK(start_waiting_time_.is_null());
  start_waiting_time_ = base::TimeTicks::Now();
}

void PageStabilityMetrics::WillMoveToState(PageStabilityMonitor::State state) {
  switch (state) {
    case PageStabilityMonitor::State::kInitial:
      NOTREACHED();
    case PageStabilityMonitor::State::kMonitorStartDelay:
      break;
    case PageStabilityMonitor::State::kWaitForNavigation:
      break;
    case PageStabilityMonitor::State::kStartMonitoring:
      start_monitoring_time_ = base::TimeTicks::Now();
      break;
    case PageStabilityMonitor::State::kTimeout:
      stability_outcome_ = PageStabilityOutcome::kTimeout;
      break;
    case PageStabilityMonitor::State::kMonitorCompleted:
      CHECK_NE(paint_stability_reached_,
               network_and_main_thread_stability_reached_);
      if (paint_stability_reached_) {
        stability_outcome_ = PageStabilityOutcome::kPaint;
      } else {
        stability_outcome_ = PageStabilityOutcome::kNetworkAndMainThread;
      }
      break;
    case PageStabilityMonitor::State::kDelayCallback:
      if (stability_outcome_ == PageStabilityOutcome::kPaint) {
        stability_outcome_ = PageStabilityOutcome::kPaintDelayed;
      } else {
        CHECK_EQ(stability_outcome_,
                 PageStabilityOutcome::kNetworkAndMainThread);
        stability_outcome_ = PageStabilityOutcome::kNetworkAndMainThreadDelayed;
      }
      break;
    case PageStabilityMonitor::State::kInvokeCallback:
      break;
    case PageStabilityMonitor::State::kRenderFrameGoingAway:
      stability_outcome_ = PageStabilityOutcome::kRenderFrameGoingAway;
      break;
    case PageStabilityMonitor::State::kMojoDisconnected:
      stability_outcome_ = PageStabilityOutcome::kMojoDisconnected;
      break;
    case PageStabilityMonitor::State::kDone:
      // The state machine is not entered until we start to wait for page
      // stability.
      CHECK(!start_waiting_time_.is_null());

      base::UmaHistogramEnumeration(
          kActorRendererPageStabilityOutcomeMetricName, stability_outcome_);

      RecordTimingMetrics();
      break;
  }
}

void PageStabilityMetrics::OnNetworkAndMainThreadIdle() {
  network_and_main_thread_stability_reached_ = true;

  CHECK(!start_monitoring_time_.is_null());
  base::UmaHistogramTimes(
      kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName,
      base::TimeTicks::Now() - start_monitoring_time_);
}

void PageStabilityMetrics::OnPaintStabilityReached() {
  paint_stability_reached_ = true;

  CHECK(!start_monitoring_time_.is_null());
  base::UmaHistogramTimes(
      kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName,
      base::TimeTicks::Now() - start_monitoring_time_);
}

void PageStabilityMetrics::RecordTimingMetrics() {
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta total_duration = now - start_waiting_time_;

  switch (stability_outcome_) {
    case PageStabilityOutcome::kNetworkAndMainThread:
    case PageStabilityOutcome::kNetworkAndMainThreadDelayed:
    case PageStabilityOutcome::kPaint:
    case PageStabilityOutcome::kPaintDelayed:
      // These are the successful stabilization paths. Record their duration to
      // analyze the performance of checks that completed as expected.
      base::UmaHistogramTimes(
          kActorRendererPageStabilityTotalTimeToStableMetricName,
          total_duration);

      CHECK(!start_monitoring_time_.is_null());
      base::UmaHistogramTimes(
          kActorRendererPageStabilityTimeFromMonitoringToStableMetricName,
          now - start_monitoring_time_);
      break;
    case PageStabilityOutcome::kRenderFrameGoingAway:
      // The check was aborted because the frame was destroyed by a navigation.
      // The duration is interesting for understanding how long checks run
      // before being navigated away.
      base::UmaHistogramTimes(
          kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName,
          total_duration);
      break;
    case PageStabilityOutcome::kUnknown:
    // TODO(linnan): This should not happen. Add NOTREACHED() after confirming
    // with the Actor.RendererPageStability.Outcome metric.
    case PageStabilityOutcome::kTimeout:
      // A timeout occurred. The duration is simply the fixed timeout value,
      // which is not an interesting distribution to measure as a timing metric.
      // The frequency of timeouts is already captured by the
      // Actor.RendererPageStability.Outcome metric count, so we do not record a
      // duration here to avoid skewing the other stats.
    case PageStabilityOutcome::kMojoDisconnected:
      // The check was aborted because the connection to the browser was lost
      // (e.g., due to a crash or shutdown). The duration until this external
      // event is not a meaningful measurement of page stability, so it is not
      // recorded.
      break;
  }
}

void PageStabilityMetrics::OnInteractionContentfulPaint() {
  const base::TimeTicks now = base::TimeTicks::Now();

  if (last_interaction_contentful_paint_time_.is_null()) {
    base::UmaHistogramTimes(
        kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName,
        now - start_waiting_time_);
  } else {
    subsequent_contentful_paint_count_++;
    total_time_between_interaction_contentful_paints_ +=
        now - last_interaction_contentful_paint_time_;
  }

  last_interaction_contentful_paint_time_ = now;
}

void PageStabilityMetrics::Flush() {
  if (flushed_) {
    return;
  }

  flushed_ = true;

  if (subsequent_contentful_paint_count_ > 0) {
    base::UmaHistogramTimes(
        kActorRendererPaintStabilityTimeBetweenInteractionContentfulPaintsMetricName,
        total_time_between_interaction_contentful_paints_ /
            subsequent_contentful_paint_count_);
    base::UmaHistogramCounts100(
        kActorRendererPaintStabilitySubsequentInteractionContentfulPaintCountMetricName,
        subsequent_contentful_paint_count_);
  }
}

}  // namespace actor

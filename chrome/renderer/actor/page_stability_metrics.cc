// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/page_stability_metrics.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/common/actor/page_stability_metrics_common.h"
#include "chrome/renderer/actor/page_stability_monitor.h"

namespace actor {

PageStabilityMetrics::PageStabilityMetrics() = default;

PageStabilityMetrics::~PageStabilityMetrics() = default;

void PageStabilityMetrics::WillMoveToState(PageStabilityMonitor::State state) {
  switch (state) {
    case PageStabilityMonitor::State::kInitial:
      NOTREACHED();
    case PageStabilityMonitor::State::kMonitorStartDelay:
      break;
    case PageStabilityMonitor::State::kWaitForNavigation:
      break;
    case PageStabilityMonitor::State::kStartMonitoring:
      break;
    case PageStabilityMonitor::State::kWaitForNetworkIdle:
      break;
    case PageStabilityMonitor::State::kWaitForMainThreadIdle:
      break;
    case PageStabilityMonitor::State::kMainThreadIdle:
      stability_outcome_ = PageStabilityOutcome::kNetworkAndMainThread;
      break;
    case PageStabilityMonitor::State::kTimeout:
      stability_outcome_ = PageStabilityOutcome::kTimeout;
      break;
    case PageStabilityMonitor::State::kMaybeDelayCallback:
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
    case PageStabilityMonitor::State::kPaintStabilityReached:
      stability_outcome_ = PageStabilityOutcome::kPaint;
      break;
    case PageStabilityMonitor::State::kMojoDisconnected:
      stability_outcome_ = PageStabilityOutcome::kMojoDisconnected;
      break;
    case PageStabilityMonitor::State::kDone:
      base::UmaHistogramEnumeration(
          kActorRendererPageStabilityOutcomeMetricName, stability_outcome_);
      break;
  }
}

}  // namespace actor

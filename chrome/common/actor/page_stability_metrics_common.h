// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_PAGE_STABILITY_METRICS_COMMON_H_
#define CHROME_COMMON_ACTOR_PAGE_STABILITY_METRICS_COMMON_H_

namespace actor {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PageStabilityOutcome)
enum class PageStabilityOutcome {
  kUnknown = 0,
  kNetworkAndMainThread = 1,
  kNetworkAndMainThreadDelayed = 2,
  kPaint = 3,
  kPaintDelayed = 4,
  kTimeout = 5,
  kRenderFrameGoingAway = 6,
  kMojoDisconnected = 7,
  kMaxValue = kMojoDisconnected,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:PageStabilityOutcome)

extern const char kActorRendererPageStabilityOutcomeMetricName[];
extern const char kActorRendererPageStabilityTotalTimeToStableMetricName[];
extern const char
    kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName[];
extern const char
    kActorRendererPageStabilityTimeFromMonitoringToStableMetricName[];
extern char const
    kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName[];
extern char const
    kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName
        [];
extern char const
    kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName
        [];
extern char const
    kActorRendererPaintStabilityTimeBetweenInteractionContentfulPaintsMetricName
        [];
extern char const
    kActorRendererPaintStabilitySubsequentInteractionContentfulPaintCountMetricName
        [];

}  // namespace actor

#endif  // CHROME_COMMON_ACTOR_PAGE_STABILITY_METRICS_COMMON_H_

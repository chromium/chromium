// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/actor/page_stability_metrics_common.h"

namespace actor {

const char kActorRendererPageStabilityOutcomeMetricName[] =
    "Actor.RendererPageStability.Outcome";

const char kActorRendererPageStabilityTotalTimeToStableMetricName[] =
    "Actor.RendererPageStability.TotalTimeToStable";

const char
    kActorRendererPageStabilityTotalTimeToRenderFrameGoingAwayMetricName[] =
        "Actor.RendererPageStability.TotalTimeToRenderFrameGoingAway";

const char kActorRendererPageStabilityTimeFromMonitoringToStableMetricName[] =
    "Actor.RendererPageStability.TimeFromMonitoringToStable";

const char
    kActorRendererPageStabilityTimeFromMonitoringToPaintStabilityMetricName[] =
        "Actor.RendererPageStability.TimeFromMonitoringToPaintStability";

const char
    kActorRendererPageStabilityTimeFromMonitoringToNetworkAndMainThreadIdleMetricName
        [] = "Actor.RendererPageStability."
             "TimeFromMonitoringToNetworkAndMainThreadIdle";

const char
    kActorRendererPaintStabilityTimeToFirstInteractionContentfulPaintMetricName
        [] = "Actor.RendererPaintStability."
             "TimeToFirstInteractionContentfulPaint";

const char
    kActorRendererPaintStabilityTimeBetweenInteractionContentfulPaintsMetricName
        [] = "Actor.RendererPaintStability."
             "TimeBetweenInteractionContentfulPaints";

const char
    kActorRendererPaintStabilitySubsequentInteractionContentfulPaintCountMetricName
        [] = "Actor.RendererPaintStability."
             "SubsequentInteractionContentfulPaintCount";

}  // namespace actor

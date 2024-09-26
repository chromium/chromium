// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_METRICS_H_
#define COMPONENTS_LENS_LENS_OVERLAY_METRICS_H_

#include <string>

#include "components/lens/lens_overlay_invocation_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace lens {

// Returns the string representation of the invocation source.
std::string InvocationSourceToString(LensOverlayInvocationSource entrypoint);

// Records the total lifetime of a lens overlay. Both unsliced and sliced.
void RecordSessionDuration(LensOverlayInvocationSource invocation_source,
                           base::TimeDelta duration);

// Records the elapsed time between when the overlay was invoked and when the
// user first interacts with the overlay. Sliced, unsliced, UMA and UKM.
void RecordTimeToFirstInteraction(LensOverlayInvocationSource invocation_source,
                                  base::TimeDelta time_to_first_interaction,
                                  ukm::SourceId source_id);

// Records UKM session end metrics.
void RecordUKMSessionEndMetrics(ukm::SourceId source_id,
                                LensOverlayInvocationSource invocation_source,
                                bool search_performed_in_session,
                                base::TimeDelta session_duration);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_METRICS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_overlay_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace lens {

std::string InvocationSourceToString(
    LensOverlayInvocationSource invocation_source) {
  switch (invocation_source) {
    case LensOverlayInvocationSource::kAppMenu:
      return "AppMenu";
    case LensOverlayInvocationSource::kContentAreaContextMenuPage:
      return "ContentAreaContextMenuPage";
    case LensOverlayInvocationSource::kContentAreaContextMenuImage:
      return "ContentAreaContextMenuImage";
    case LensOverlayInvocationSource::kToolbar:
      return "Toolbar";
    case LensOverlayInvocationSource::kFindInPage:
      return "FindInPage";
    case LensOverlayInvocationSource::kOmnibox:
      return "Omnibox";
  }
}

void RecordSessionDuration(LensOverlayInvocationSource invocation_source,
                           base::TimeDelta duration) {
  // UMA unsliced session duration.
  base::UmaHistogramCustomTimes("Lens.Overlay.SessionDuration", duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);

  // UMA session duration sliced by entry point.
  const auto sliced_session_duration_histogram_name =
      "Lens.Overlay.ByInvocationSource." +
      InvocationSourceToString(invocation_source) + ".SessionDuration";
  base::UmaHistogramCustomTimes(sliced_session_duration_histogram_name,
                                duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTimeToFirstInteraction(LensOverlayInvocationSource invocation_source,
                                  base::TimeDelta time_to_first_interaction,
                                  ukm::SourceId source_id) {
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes("Lens.Overlay.TimeToFirstInteraction",
                                time_to_first_interaction,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
  // UMA TimeToFirstInteraction sliced by entry point.
  const auto sliced_time_to_first_interaction_histogram_name =
      "Lens.Overlay.ByInvocationSource." +
      InvocationSourceToString(invocation_source) + ".TimeToFirstInteraction";
  base::UmaHistogramCustomTimes(sliced_time_to_first_interaction_histogram_name,
                                time_to_first_interaction,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);

  // UKM unsliced TimeToFirstInteraction.
  ukm::builders::Lens_Overlay_TimeToFirstInteraction(source_id)
      .SetAllEntryPoints(time_to_first_interaction.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
  // UKM TimeToFirstInteraction sliced by entry point.
  ukm::builders::Lens_Overlay_TimeToFirstInteraction event(source_id);
  switch (invocation_source) {
    case lens::LensOverlayInvocationSource::kAppMenu:
      event.SetAppMenu(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuPage:
      event.SetContentAreaContextMenuPage(
          time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuImage:
      // Not recorded since the image menu entry point results in a search
      // without the user having to interact with the overlay. Time to first
      // interaction in this case is essentially zero.
      break;
    case lens::LensOverlayInvocationSource::kToolbar:
      event.SetToolbar(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kFindInPage:
      event.SetFindInPage(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kOmnibox:
      event.SetOmnibox(time_to_first_interaction.InMilliseconds());
      break;
  }
  event.Record(ukm::UkmRecorder::Get());
}

void RecordUKMSessionEndMetrics(ukm::SourceId source_id,
                                LensOverlayInvocationSource invocation_source,
                                bool search_performed_in_session,
                                base::TimeDelta session_duration) {
  ukm::builders::Lens_Overlay_SessionEnd(source_id)
      .SetInvocationSource(static_cast<int64_t>(invocation_source))
      .SetInvocationResultedInSearch(search_performed_in_session)
      .SetSessionDuration(session_duration.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace lens

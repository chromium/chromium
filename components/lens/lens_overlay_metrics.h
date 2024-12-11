// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_METRICS_H_
#define COMPONENTS_LENS_LENS_OVERLAY_METRICS_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_first_interaction_type.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_new_tab_source.h"
#include "components/lens/lens_overlay_side_panel_result.h"
#include "components/lens/lens_permission_user_action.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace lens {

// Returns the string representation of the invocation source.
std::string InvocationSourceToString(
    LensOverlayInvocationSource invocation_source);

// Returns the string representation of the page content type.
std::string DocumentTypeToString(lens::MimeType page_content_type);

// Recorded when lens permission is requested to be shown. Both sliced and
// unsliced.
void RecordPermissionRequestedToBeShown(
    bool shown,
    LensOverlayInvocationSource invocation_source);

// Records user action in lens permission. Both sliced and unsliced.
void RecordPermissionUserAction(LensPermissionUserAction user_action,
                                LensOverlayInvocationSource invocation_source);

// Records lens overlay invocation.
void RecordInvocation(LensOverlayInvocationSource invocation_source,
                      lens::MimeType page_content_type);

// Records lens overlay dismissal.
void RecordDismissal(LensOverlayDismissalSource dismissal_source);

// Records whether or not a search was performed at any point since the
// overlay was invoked. Both sliced and unsliced.
void RecordInvocationResultedInSearch(
    LensOverlayInvocationSource invocation_source,
    bool search_performed_in_session);

// Records the total lifetime of a lens overlay. Both unsliced and sliced.
void RecordSessionDuration(LensOverlayInvocationSource invocation_source,
                           base::TimeDelta duration);

// Records the end of sessions metrics for the contextual searchbox in sessions
// in which it was shown.
void RecordContextualSearchboxSessionEndMetrics(
    ukm::SourceId source_id,
    bool contextual_searchbox_shown_in_session,
    bool contextual_searchbox_focused_in_session,
    bool contextual_zps_shown_in_session,
    bool contextual_zps_used_in_session,
    bool contextual_query_issued_in_session,
    lens::MimeType page_content_type);

// Records the time in foreground of a lens overlay. Both sliced and unsliced.
void RecordSessionForegroundDuration(
    LensOverlayInvocationSource invocation_source,
    base::TimeDelta duration);

// Records the elapsed time between when the overlay was invoked and when the
// user first interacts with the overlay. Sliced, unsliced, UMA and UKM.
void RecordTimeToFirstInteraction(
    LensOverlayInvocationSource invocation_source,
    base::TimeDelta time_to_first_interaction,
    LensOverlayFirstInteractionType first_interaction_type,
    ukm::SourceId source_id);

// Records that a new tab has been generated from `tab_source`.
void RecordNewTabGenerated(LensOverlayNewTabSource tab_source);

// Records the number of tabs generated during the total lifetime of a lens
// overlay.
void RecordGeneratedTabCount(int generated_tab_count);

// Records UKM session end metrics. `session_foreground_duration` and
// `generated_tab_count` are only recorded on iOS. Remove the optional when
// recording on Desktop.
void RecordUKMSessionEndMetrics(
    ukm::SourceId source_id,
    LensOverlayInvocationSource invocation_source,
    bool search_performed_in_session,
    base::TimeDelta session_duration,
    lens::MimeType document_content_type,
    std::optional<base::TimeDelta> session_foreground_duration = std::nullopt,
    std::optional<int> generated_tab_count = std::nullopt);

// Records the duration between the time a lens request is started and the time
// a response is generated.
void RecordLensResponseTime(base::TimeDelta response_time);

// Records the time from the time the user navigates the document to when the
// contextual search box is interacted with, sliced by content type.
void RecordContextualSearchboxTimeToInteractionAfterNavigation(
    base::TimeDelta time_to_interaction,
    lens::MimeType page_content_type);

// Records the size of the document where the contextual search box was shown,
// sliced by content type.
void RecordDocumentSizeBytes(lens::MimeType page_content_type,
                             size_t document_size_bytes);

// Record the number of pages in a PDF.
void RecordPdfPageCount(uint32_t page_count);

// Records the side panel result status when attempting a load into the side
// panel.
void RecordSidePanelResultStatus(SidePanelResultStatus status);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_METRICS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_OVERLAY_METRICS_H_
#define COMPONENTS_LENS_LENS_OVERLAY_METRICS_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/lens/lens_composebox_user_action.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_first_interaction_type.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_new_tab_source.h"
#include "components/lens/lens_overlay_non_blocking_privacy_notice_user_action.h"
#include "components/lens/lens_overlay_side_panel_menu_option.h"
#include "components/lens/lens_overlay_side_panel_result.h"
#include "components/lens/lens_permission_user_action.h"
#include "net/base/net_errors.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace lens {

struct ContextualSearchboxSessionEndMetrics {
  // Indicates whether zps was shown for the initial query in a session.
  bool zps_shown_on_initial_query_ = false;

  // Indicates whether zps was shown for the initial query in a session.
  bool zps_shown_on_follow_up_query_ = false;

  // Indicates whether contextual zero suggest was used in a session.
  bool zps_used_ = false;

  // Indicates whether a contextual query was issued in a session.
  bool query_issued_ = false;

  // Indicates whether a follo up contextual query was issued in a session.
  bool follow_up_query_issued_ = false;

  // Indicates whether a contextual query was issued before zps was shown for
  // the initial query in a session.
  bool initial_query_issued_before_zps_shown_ = false;

  // Indicates whether a contextual query was issued before zps was shown for
  // the follow up query in a session.
  bool follow_up_query_issued_before_zps_shown_ = false;

  // Indicates whether the contextual searchbox was focused in the current
  // session. Used to record interaction rate, defined by whether or not a
  // user focused the contextual searchbox in sessions in which it was shown.
  // Set if contextual searchbox is shown.
  bool searchbox_focused_ = false;

  // Whether the contextual searchbox should be shown in the session.
  bool searchbox_shown_ = false;
};

struct AimSessionEndMetrics {
  // Indicates whether the AIM searchbox was shown in the session.
  bool composebox_shown_ = false;

  // Indicates whether the AIM handshake was received in the session.
  bool handshake_completed_ = false;

  // Indicates whether the AIM searchbox was focused in the session.
  bool composebox_focused_ = false;

  // Indicates whether a query was issued in the session.
  bool query_issued_ = false;

  // Indicates whether a query was submitted in the session.
  bool query_submitted_ = false;
};

// LINT.IfChange(LensOverlayTextDirectiveResult)
enum class LensOverlayTextDirectiveResult {
  // The text directive was found on the page.
  kFoundOnPage = 0,
  // The URL with a text directive was opened in a new tab because it did not
  // match the current page.
  kOpenedInNewTab = 1,
  // The text directive was not found on the page.
  kNotFoundOnPage = 2,
  kMaxValue = kNotFoundOnPage,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/lens/enums.xml:LensOverlayTextDirectiveResult)

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

// Recorded when non-blocking privacy notice is requested to be shown.
void RecordNonBlockingPrivacyNoticeToBeShown(
    LensOverlayInvocationSource invocation_source);

// Recorded when a user interaction causes the non-blocking privacy notice to be
// accepted.
void RecordNonBlockingPrivacyNoticeAccepted(
    LensOverlayNonBlockingPrivacyNoticeUserAction user_action,
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
// in which it was shown. `page_content_type` is the mime type of the content
// that was extracted from the page when the contextual searchbox was shown.
// `document_content_type` is the type of the document that the user invoked
// Lens on, as determined by the WebContents.
void RecordContextualSearchboxSessionEndMetrics(
    ukm::SourceId source_id,
    ContextualSearchboxSessionEndMetrics session_end_metrics,
    lens::MimeType page_content_type,
    lens::MimeType document_content_type);

// Records the end of sessions metrics for the AIM searchbox.
void RecordAimSessionEndMetrics(AimSessionEndMetrics aim_session_end_metrics);

// Records user action in the AIM composebox.
void RecordAimComposeboxUserAction(LensComposeboxUserAction user_action);

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

// Records the time between the overlay is invoked and the contextual searchbox
// is first focused.
void RecordContextualSearchboxTimeToFirstFocus(
    base::TimeDelta time_to_focus,
    lens::MimeType page_content_type);

// Records the time from the time the user navigates the document to when the
// contextual search box is interacted with, sliced by page content type.
void RecordContextualSearchboxTimeToFocusAfterNavigation(
    base::TimeDelta time_to_focus,
    lens::MimeType page_content_type);

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

// Records the similarity between the OCR text and the DOM text. Similarity is
// a value between 0 and 1.
void RecordOcrDomSimilarity(double similarity);

// Records the side panel result status when attempting a load into the side
// panel.
void RecordSidePanelResultStatus(SidePanelResultStatus status);

// Records that a side panel menu option has been selected.
void RecordSidePanelMenuOptionSelected(
    lens::LensOverlaySidePanelMenuOption menu_option);

// Records the result of handling a text directive in the Lens Overlay.
void RecordHandleTextDirectiveResult(
    lens::LensOverlayTextDirectiveResult result);

// Records the load status of the side panel iframe.
void RecordIframeLoadStatus(bool is_error_page, net::Error net_error_code);

// Records the time it takes to close the side panel
void RecordTimeToCloseOpenedSidePanel(base::TimeDelta duration);

// Records the time it takes to take a screenshot.
void RecordTimeToScreenshot(base::TimeDelta duration);

// Records the time it takes to fetch bounding boxes.
void RecordTimeToFetchBoundingBoxes(base::TimeDelta duration);

// Records the time it takes to fetch the PDF page.
void RecordTimeToFetchPdfPage(base::TimeDelta duration);

// Records the time it takes to check page context eligibility.
void RecordTimeToCheckPageContextEligibility(base::TimeDelta duration);

// Records the time it takes to create the screenshot bitmap.
void RecordTimeToCreateScreenshotBitmap(base::TimeDelta duration);

// Records the time it takes to get the page context
void RecordTimeToGetPageContext(base::TimeDelta duration);

// Records the time it takes for the page to bind
void RecordTimeToWebuiBound(base::TimeDelta duration);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_OVERLAY_METRICS_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_overlay_metrics.h"

#include <cstddef>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "components/lens/lens_composebox_user_action.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_non_blocking_privacy_notice_user_action.h"
#include "components/lens/lens_side_panel_iframe_load_status.h"
#include "net/base/net_errors.h"
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
    case LensOverlayInvocationSource::kLVFShutterButton:
      return "LVFShutterButton";
    case LensOverlayInvocationSource::kLVFGallery:
      return "LVFGallery";
    case LensOverlayInvocationSource::kContextMenu:
      return "ContextMenu";
    case LensOverlayInvocationSource::kOmniboxPageAction:
      return "OmniboxPageAction";
    case LensOverlayInvocationSource::kOmniboxContextualSuggestion:
      return "OmniboxContextualSuggestion";
    case LensOverlayInvocationSource::kHomeworkActionChip:
      return "HomeworkActionChip";
    case LensOverlayInvocationSource::kAIHub:
      return "AIHub";
    case LensOverlayInvocationSource::kFREPromo:
      return "FREPromo";
    case LensOverlayInvocationSource::kContentAreaContextMenuText:
      return "ContentAreaContextMenuText";
    case LensOverlayInvocationSource::kContentAreaContextMenuVideo:
      return "ContentAreaContextMenuVideo";
  }
}

std::string FirstInteractionTypeToString(
    LensOverlayFirstInteractionType interaction_type) {
  switch (interaction_type) {
    case LensOverlayFirstInteractionType::kPermissionDialog:
      return "Permission";
    case LensOverlayFirstInteractionType::kLensMenu:
      return "LensMenu";
    case LensOverlayFirstInteractionType::kRegionSelect:
      return "RegionSelect";
    case LensOverlayFirstInteractionType::kTextSelect:
      return "TextSelect";
    case LensOverlayFirstInteractionType::kSearchbox:
      return "Searchbox";
  }
}

std::string MimeTypeToMetricString(lens::MimeType mime_type) {
  switch (mime_type) {
    case lens::MimeType::kPdf:
      return "Pdf";
    case lens::MimeType::kHtml:
      return "Html";
    case lens::MimeType::kPlainText:
      return "PlainText";
    case lens::MimeType::kAnnotatedPageContent:
      return "AnnotatedPageContent";
    case lens::MimeType::kImage:
      return "Image";
    case lens::MimeType::kVideo:
      return "Video";
    case lens::MimeType::kAudio:
      return "Audio";
    case lens::MimeType::kJson:
      return "Json";
    default:
      return "Unknown";
  }
}

void RecordPermissionRequestedToBeShown(
    bool shown,
    LensOverlayInvocationSource invocation_source) {
  base::UmaHistogramBoolean("Lens.Overlay.PermissionBubble.Shown", shown);
  const auto histogram_name =
      "Lens.Overlay.PermissionBubble.ByInvocationSource." +
      InvocationSourceToString(invocation_source) + ".Shown";
  base::UmaHistogramBoolean(histogram_name, shown);
}

void RecordPermissionUserAction(LensPermissionUserAction user_action,
                                LensOverlayInvocationSource invocation_source) {
  base::UmaHistogramEnumeration("Lens.Overlay.PermissionBubble.UserAction",
                                user_action);
  const auto histogram_name =
      "Lens.Overlay.PermissionBubble.ByInvocationSource." +
      InvocationSourceToString(invocation_source) + ".UserAction";
  base::UmaHistogramEnumeration(histogram_name, user_action);
}

void RecordNonBlockingPrivacyNoticeToBeShown(
    LensOverlayInvocationSource invocation_source) {
  base::UmaHistogramEnumeration("Lens.Overlay.NonBlockingPrivacyNotice.Shown",
                                invocation_source);
}

void RecordNonBlockingPrivacyNoticeAccepted(
    LensOverlayNonBlockingPrivacyNoticeUserAction user_action,
    LensOverlayInvocationSource invocation_source) {
  base::UmaHistogramEnumeration(
      "Lens.Overlay.NonBlockingPrivacyNotice.Accepted", user_action);
  const auto histogram_name =
      "Lens.Overlay.NonBlockingPrivacyNotice.ByInvocationSource." +
      InvocationSourceToString(invocation_source) + ".Accepted";
  base::UmaHistogramEnumeration(histogram_name, user_action);
}

void RecordInvocation(LensOverlayInvocationSource invocation_source,
                      lens::MimeType document_content_type) {
  base::UmaHistogramEnumeration("Lens.Overlay.Invoked", invocation_source);

  // UMA Invocation sliced by document type.
  const auto sliced_invoked_histogram_name =
      "Lens.Overlay.ByDocumentType." +
      MimeTypeToMetricString(document_content_type) + ".Invoked";
  base::UmaHistogramBoolean(sliced_invoked_histogram_name, true);
}

void RecordDismissal(LensOverlayDismissalSource dismissal_source) {
  base::UmaHistogramEnumeration("Lens.Overlay.Dismissed", dismissal_source);
}

void RecordInvocationResultedInSearch(
    LensOverlayInvocationSource invocation_source,
    bool search_performed_in_session) {
  // UMA unsliced InvocationResultedInSearch.
  base::UmaHistogramBoolean("Lens.Overlay.InvocationResultedInSearch",
                            search_performed_in_session);

  // UMA InvocationResultedInSearch sliced by entry point.
  const auto sliced_search_performed_histogram_name =
      "Lens.Overlay.ByInvocationSource." +
      InvocationSourceToString(invocation_source) +
      ".InvocationResultedInSearch";
  base::UmaHistogramBoolean(sliced_search_performed_histogram_name,
                            search_performed_in_session);
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

void RecordContextualSearchboxSessionEndMetrics(
    ukm::SourceId source_id,
    ContextualSearchboxSessionEndMetrics session_end_metrics,
    lens::MimeType page_content_type,
    lens::MimeType document_content_type) {
  // UMA contextual searchbox shown in session.
  base::UmaHistogramBoolean("Lens.Overlay.ContextualSearchBox.ShownInSession",
                            session_end_metrics.searchbox_shown_);

  // UMA contextual searchbox shown in session sliced by page content type.
  const auto sliced_page_content_invoked_histogram_name =
      "Lens.Overlay.ContextualSearchBox.ByPageContentType." +
      MimeTypeToMetricString(page_content_type) + ".ShownInSession";
  base::UmaHistogramBoolean(sliced_page_content_invoked_histogram_name,
                            session_end_metrics.searchbox_shown_);

  // UMA contextual searchbox shown in session sliced by document type.
  const auto sliced_document_invoked_histogram_name =
      "Lens.Overlay.ContextualSearchBox.ByDocumentType." +
      MimeTypeToMetricString(document_content_type) + ".ShownInSession";
  base::UmaHistogramBoolean(sliced_document_invoked_histogram_name,
                            session_end_metrics.searchbox_shown_);

  // Record UKM for contextual search box shown in session.
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession event(
        source_id);
    event.SetWasShown(session_end_metrics.searchbox_shown_)
        .SetPageContentType(static_cast<int64_t>(page_content_type))
        .Record(ukm::UkmRecorder::Get());
  }

  // Don't record the rest of the contextual searchbox metrics if it was not
  // shown in the session.
  if (!session_end_metrics.searchbox_shown_) {
    return;
  }

  // UMA contextual searchbox focused in session.
  base::UmaHistogramBoolean("Lens.Overlay.ContextualSearchBox.FocusedInSession",
                            session_end_metrics.searchbox_focused_);
  // UMA contextual searchbox focused in session sliced by document type.
  const auto sliced_focused_histogram_name =
      "Lens.Overlay.ContextualSearchBox.ByPageContentType." +
      MimeTypeToMetricString(page_content_type) + ".FocusedInSession";
  base::UmaHistogramBoolean(sliced_focused_histogram_name,
                            session_end_metrics.searchbox_focused_);

  bool zps_shown_in_session = session_end_metrics.zps_shown_on_initial_query_ ||
                              session_end_metrics.zps_shown_on_follow_up_query_;
  // UMA contextual zps shown in session.
  base::UmaHistogramBoolean("Lens.Overlay.ContextualSuggest.ZPS.ShownInSession",
                            zps_shown_in_session);
  // UMA contextual zps shown in session sliced by document type.
  const auto sliced_contextual_zps_histogram_name =
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType." +
      MimeTypeToMetricString(page_content_type) + ".ShownInSession";
  base::UmaHistogramBoolean(sliced_contextual_zps_histogram_name,
                            zps_shown_in_session);

  // UMA contextual zps used in session.
  base::UmaHistogramBoolean(
      "Lens.Overlay.ContextualSuggest.ZPS.SuggestionUsedInSession",
      session_end_metrics.zps_used_);
  // UMA contextual zps used in session sliced by document type.
  const auto sliced_contextual_zps_used_histogram_name =
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType." +
      MimeTypeToMetricString(page_content_type) + ".SuggestionUsedInSession";
  base::UmaHistogramBoolean(sliced_contextual_zps_used_histogram_name,
                            session_end_metrics.zps_used_);

  // UMA contextual query issued in session.
  base::UmaHistogramBoolean(
      "Lens.Overlay.ContextualSuggest.QueryIssuedInSession",
      session_end_metrics.query_issued_);
  // UMA contextual query issued in session sliced by document type.
  const auto sliced_contextual_query_issued_histogram_name =
      "Lens.Overlay.ContextualSuggest.ByPageContentType." +
      MimeTypeToMetricString(page_content_type) + ".QueryIssuedInSession";
  base::UmaHistogramBoolean(sliced_contextual_query_issued_histogram_name,
                            session_end_metrics.query_issued_);

  // Only record the contextual query issued before zps shown metrics if a
  // contextual query was issued in the session.
  if (session_end_metrics.query_issued_) {
    // UMA initial contextual query issued before zps shown in session.
    base::UmaHistogramBoolean(
        "Lens.Overlay.ContextualSuggest.InitialQuery."
        "QueryIssuedInSessionBeforeSuggestShown",
        session_end_metrics.initial_query_issued_before_zps_shown_);
    // UMA initial contextual query issued before zps shown in session sliced by
    // page content type.
    const auto
        sliced_contextual_query_issued_before_zps_shown_initial_histogram_name =
            "Lens.Overlay.ContextualSuggest.InitialQuery.ByPageContentType." +
            MimeTypeToMetricString(page_content_type) +
            ".QueryIssuedInSessionBeforeSuggestShown";
    base::UmaHistogramBoolean(
        sliced_contextual_query_issued_before_zps_shown_initial_histogram_name,
        session_end_metrics.initial_query_issued_before_zps_shown_);
  }

  if (session_end_metrics.follow_up_query_issued_) {
    // UMA follow up contextual query issued before zps shown in session.
    base::UmaHistogramBoolean(
        "Lens.Overlay.ContextualSuggest.FollowUpQuery."
        "QueryIssuedInSessionBeforeSuggestShown",
        session_end_metrics.follow_up_query_issued_before_zps_shown_);
    // UMA initial contextual query issued before zps shown in session sliced by
    // page content type.
    const auto
        sliced_contextual_query_issued_before_zps_shown_follow_up_histogram_name =
            "Lens.Overlay.ContextualSuggest.FollowUpQuery.ByPageContentType." +
            MimeTypeToMetricString(page_content_type) +
            ".QueryIssuedInSessionBeforeSuggestShown";
    base::UmaHistogramBoolean(
        sliced_contextual_query_issued_before_zps_shown_follow_up_histogram_name,
        session_end_metrics.follow_up_query_issued_before_zps_shown_);
  }

  if (source_id == ukm::kInvalidSourceId) {
    return;
  }

  // UKM contextual searchbox focused in session.
  ukm::builders::Lens_Overlay_ContextualSearchbox_FocusedInSession(source_id)
      .SetFocusedInSession(session_end_metrics.searchbox_focused_)
      .SetPageContentType(static_cast<int64_t>(page_content_type))
      .Record(ukm::UkmRecorder::Get());

  // UKM contextual zps shown in session.
  ukm::builders::Lens_Overlay_ContextualSuggest_ZPS_ShownInSession(source_id)
      .SetShownInSession(zps_shown_in_session)
      .SetPageContentType(static_cast<int64_t>(page_content_type))
      .Record(ukm::UkmRecorder::Get());

  // UKM contextual zps used in session.
  ukm::builders::Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession(
      source_id)
      .SetSuggestionUsedInSession(session_end_metrics.zps_used_)
      .SetPageContentType(static_cast<int64_t>(page_content_type))
      .Record(ukm::UkmRecorder::Get());

  // UKM contextual query issued in session.
  ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession(source_id)
      .SetQueryIssuedInSession(session_end_metrics.query_issued_)
      .SetPageContentType(static_cast<int64_t>(page_content_type))
      .Record(ukm::UkmRecorder::Get());
}

void RecordAimSessionEndMetrics(AimSessionEndMetrics aim_session_end_metrics) {
  // UMA AIM searchbox shown in session.
  base::UmaHistogramBoolean("Lens.Composebox.ShownInSession",
                            aim_session_end_metrics.composebox_shown_);
  if (!aim_session_end_metrics.composebox_shown_) {
    return;
  }

  // UMA AIM communication handshake completed in session.
  base::UmaHistogramBoolean("Lens.Composebox.HandshakeCompletedInSession",
                            aim_session_end_metrics.handshake_completed_);

  // UMA AIM searchbox focused in session.
  if (aim_session_end_metrics.composebox_focused_) {
    base::UmaHistogramEnumeration("Lens.Composebox.UserActionInSession",
                                  LensComposeboxUserAction::kFocused);
  }

  // UMA AIM searchbox query submitted in session.
  if (aim_session_end_metrics.query_submitted_) {
    base::UmaHistogramEnumeration("Lens.Composebox.UserActionInSession",
                                  LensComposeboxUserAction::kQuerySubmitted);
  }

  // UMA AIM searchbox query issued in session.
  if (aim_session_end_metrics.query_issued_) {
    base::UmaHistogramEnumeration("Lens.Composebox.UserActionInSession",
                                  LensComposeboxUserAction::kQueryIssued);
  }
}

void RecordAimComposeboxUserAction(LensComposeboxUserAction user_action) {
  base::UmaHistogramEnumeration("Lens.Composebox.UserAction", user_action);
}

void RecordSessionForegroundDuration(
    LensOverlayInvocationSource invocation_source,
    base::TimeDelta duration) {
  // UMA unsliced session duration.
  base::UmaHistogramCustomTimes("Lens.Overlay.SessionForegroundDuration",
                                duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);

  // UMA session duration sliced by entry point.
  const auto sliced_session_duration_histogram_name =
      "Lens.Overlay.ByInvocationSource." +
      InvocationSourceToString(invocation_source) +
      ".SessionForegroundDuration";
  base::UmaHistogramCustomTimes(sliced_session_duration_histogram_name,
                                duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTimeToFirstInteraction(
    LensOverlayInvocationSource invocation_source,
    base::TimeDelta time_to_first_interaction,
    LensOverlayFirstInteractionType first_interaction_type,
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

  // UMA TimeToFirstInteraction sliced by interaction type.
  const auto interaction_type_histogram_name =
      "Lens.Overlay.TimeToFirstInteraction." +
      FirstInteractionTypeToString(first_interaction_type);

  base::UmaHistogramCustomTimes(interaction_type_histogram_name,
                                time_to_first_interaction,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);

  if (source_id == ukm::kInvalidSourceId) {
    return;
  }

  // UKM unsliced TimeToFirstInteraction.
  ukm::builders::Lens_Overlay_TimeToFirstInteraction(source_id)
      .SetAllEntryPoints(time_to_first_interaction.InMilliseconds())
      .SetFirstInteractionType(static_cast<int64_t>(first_interaction_type))
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
    case LensOverlayInvocationSource::kLVFShutterButton:
    case LensOverlayInvocationSource::kLVFGallery:
    case LensOverlayInvocationSource::kContextMenu:
      // Not recorded since for LVF and context menu invocation the first
      // interaction is done automatically by autoselection.
      break;
    case lens::LensOverlayInvocationSource::kToolbar:
      event.SetToolbar(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kFindInPage:
      event.SetFindInPage(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kOmnibox:
    case lens::LensOverlayInvocationSource::kAIHub:
      event.SetOmnibox(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kOmniboxPageAction:
      event.SetOmniboxPageAction(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kOmniboxContextualSuggestion:
      event.SetOmniboxContextualSuggestion(
          time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kFREPromo:
      // First interaction for Lens Overlay is already recorded and sliced by
      // invocation source.
      break;
    case lens::LensOverlayInvocationSource::kHomeworkActionChip:
      event.SetHomeworkActionChip(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuText:
      event.SetContentAreaContextMenuText(
          time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuVideo:
      // Not recorded since the video context menu entry point results in a
      // search without the user having to interact with the overlay. Time to
      // first interaction in this case is essentially zero.
      break;
  }
  event.SetFirstInteractionType(static_cast<int64_t>(first_interaction_type))
      .Record(ukm::UkmRecorder::Get());
}

void RecordNewTabGenerated(LensOverlayNewTabSource tab_source) {
  base::UmaHistogramEnumeration("Lens.Overlay.GeneratedTab", tab_source);
}

void RecordGeneratedTabCount(int generated_tab_count) {
  base::UmaHistogramCounts100("Lens.Overlay.GeneratedTab.SessionCount",
                              generated_tab_count);
}

void RecordUKMSessionEndMetrics(
    ukm::SourceId source_id,
    LensOverlayInvocationSource invocation_source,
    bool search_performed_in_session,
    base::TimeDelta session_duration,
    lens::MimeType document_content_type,
    std::optional<base::TimeDelta> session_foreground_duration,
    std::optional<int> generated_tab_count) {
  if (source_id == ukm::kInvalidSourceId) {
    return;
  }
  ukm::builders::Lens_Overlay_SessionEnd event(source_id);

  if (session_foreground_duration.has_value()) {
    event.SetSessionForegroundDuration(
        session_foreground_duration->InMilliseconds());
  }
  if (generated_tab_count.has_value()) {
    event.SetGeneratedTabCount(*generated_tab_count);
  }

  event.SetInvocationSource(static_cast<int64_t>(invocation_source))
      .SetInvocationResultedInSearch(search_performed_in_session)
      .SetSessionDuration(session_duration.InMilliseconds())
      .SetInvocationDocumentType(static_cast<int64_t>(document_content_type))
      .Record(ukm::UkmRecorder::Get());
}

void RecordLensResponseTime(base::TimeDelta response_time) {
  base::UmaHistogramTimes("Lens.Overlay.LensResponseTime", response_time);
}

void RecordContextualSearchboxTimeToFirstFocus(
    base::TimeDelta time_to_focus,
    lens::MimeType page_content_type) {
  const auto sliced_time_to_interaction_histogram_name =
      "Lens.Overlay.ContextualSearchBox.ByPageContentType." +
      MimeTypeToMetricString(page_content_type) +
      ".TimeFromInvocationToFirstFocus";
  base::UmaHistogramCustomTimes(sliced_time_to_interaction_histogram_name,
                                time_to_focus,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(5), /*buckets=*/50);
}

void RecordContextualSearchboxTimeToFocusAfterNavigation(
    base::TimeDelta time_to_focus,
    lens::MimeType page_content_type) {
  const auto sliced_time_to_interaction_histogram_name =
      "Lens.Overlay.ContextualSearchBox.ByPageContentType." +
      MimeTypeToMetricString(page_content_type) +
      ".TimeFromNavigationToFirstFocus";
  base::UmaHistogramCustomTimes(sliced_time_to_interaction_histogram_name,
                                time_to_focus,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordContextualSearchboxTimeToInteractionAfterNavigation(
    base::TimeDelta time_to_interaction,
    lens::MimeType page_content_type) {
  const auto sliced_time_to_interaction_histogram_name =
      "Lens.Overlay.ContextualSearchBox.ByPageContentType." +
      MimeTypeToMetricString(page_content_type) +
      ".TimeFromNavigationToFirstInteraction";
  base::UmaHistogramCustomTimes(sliced_time_to_interaction_histogram_name,
                                time_to_interaction,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordDocumentSizeBytes(lens::MimeType page_content_type,
                             size_t document_size_bytes) {
  const auto sliced_invoked_histogram_name =
      "Lens.Overlay.ByPageContentType." +
      MimeTypeToMetricString(page_content_type) + ".DocumentSize2";
  base::UmaHistogramCustomCounts(sliced_invoked_histogram_name,
                                 document_size_bytes / 1000, 1, 100000, 100);
}

void RecordPdfPageCount(uint32_t page_count) {
  base::UmaHistogramCounts10000("Lens.Overlay.ByPageContentType.Pdf.PageCount",
                                page_count);
}

void RecordOcrDomSimilarity(double similarity) {
  base::UmaHistogramPercentage("Lens.Overlay.OcrDomSimilarity",
                               similarity * 100);
}

void RecordSidePanelResultStatus(SidePanelResultStatus status) {
  base::UmaHistogramEnumeration("Lens.Overlay.SidePanelResultStatus", status);
}

void RecordSidePanelMenuOptionSelected(
    lens::LensOverlaySidePanelMenuOption menu_option) {
  base::UmaHistogramEnumeration(
      "Lens.Overlay.SidePanel.SelectedMoreInfoMenuOption", menu_option);
}

void RecordHandleTextDirectiveResult(
    lens::LensOverlayTextDirectiveResult result) {
  base::UmaHistogramEnumeration("Lens.Overlay.TextDirectiveResult", result);
}

void RecordIframeLoadStatus(bool is_error_page, net::Error net_error_code) {
  lens::IframeLoadStatus status = lens::IframeLoadStatus::kSuccess;
  if (is_error_page) {
    switch (net_error_code) {
      case net::ERR_CONNECTION_REFUSED:
        status = lens::IframeLoadStatus::kFailedConnectionRefused;
        break;
      case net::ERR_CONNECTION_RESET:
        status = lens::IframeLoadStatus::kFailedConnectionReset;
        break;
      case net::ERR_CONNECTION_TIMED_OUT:
        status = lens::IframeLoadStatus::kFailedConnectionTimedOut;
        break;
      case net::ERR_TIMED_OUT:
        status = lens::IframeLoadStatus::kFailedTimedOut;
        break;
      case net::ERR_NAME_NOT_RESOLVED:
        status = lens::IframeLoadStatus::kFailedNameNotResolved;
        break;
      default:
        status = lens::IframeLoadStatus::kFailedOther;
        break;
    }
  }
  base::UmaHistogramEnumeration("Lens.Overlay.SidePanel.IframeLoadStatus",
                                status);
}

void RecordTimeToCloseOpenedSidePanel(base::TimeDelta duration) {
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes("Lens.Overlay.TimeToCloseOpenedSidePanel",
                                duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTimeToScreenshot(base::TimeDelta duration) {
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes("Lens.Overlay.TimeToScreenshot", duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTimeToFetchBoundingBoxes(base::TimeDelta duration) {
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes("Lens.Overlay.TimeToFetchBoundingBoxes",
                                duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTimeToFetchPdfPage(base::TimeDelta duration) {
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes("Lens.Overlay.TimeToFetchPdfPage", duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTimeToCheckPageContextEligibility(base::TimeDelta duration) {
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes(
      "Lens.Overlay.TimeToCheckPageContextEligibility", duration,
      /*min=*/base::Milliseconds(1),
      /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTimeToCreateScreenshotBitmap(base::TimeDelta duration) {
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes("Lens.Overlay.TimeToCreateScreenshotBitmap",
                                duration, /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTimeToGetPageContext(base::TimeDelta duration) {
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes("Lens.Overlay.TimeToGetPageContext", duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTimeToWebuiBound(base::TimeDelta duration) {
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes("Lens.Overlay.TimeToWebuiBound", duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

}  // namespace lens

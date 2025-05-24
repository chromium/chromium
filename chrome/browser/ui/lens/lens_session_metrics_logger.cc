// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"

#include "base/time/time.h"
#include "chrome/browser/content_extraction/inner_html.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/page_content_type_conversions.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace lens {

LensSessionMetricsLogger::LensSessionMetricsLogger() = default;
LensSessionMetricsLogger::~LensSessionMetricsLogger() = default;

void LensSessionMetricsLogger::OnSessionStart(
    LensOverlayInvocationSource invocation_source,
    content::WebContents* tab_web_contents) {
  invocation_source_ = invocation_source;
  invocation_time_ = base::TimeTicks::Now();
  search_performed_in_session_ = false;
  if (tab_web_contents && tab_web_contents->GetPrimaryMainFrame()) {
    this->ukm_source_id_ =
        tab_web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  }
  initial_document_type_ = lens::StringMimeTypeToDocumentType(
      tab_web_contents->GetContentsMimeType());
  csb_session_end_metrics_ = {};
}

void LensSessionMetricsLogger::OnPageNavigation() {
  last_navigation_time_ = base::TimeTicks::Now();
  contextual_searchbox_focused_after_navigation_ = false;
}

void LensSessionMetricsLogger::OnSearchPerformed() {
  search_performed_in_session_ = true;
}

void LensSessionMetricsLogger::OnInitialPageContentRetrieved(
    lens::MimeType page_content_type) {
  initial_page_content_type_ = page_content_type;
  current_page_content_type_ = page_content_type;
}

void LensSessionMetricsLogger::OnFollowUpPageContentRetrieved(
    lens::MimeType page_content_type) {
  current_page_content_type_ = page_content_type;
}

void LensSessionMetricsLogger::OnContextualSearchboxShown() {
  csb_session_end_metrics_.searchbox_shown_ = true;
}

void LensSessionMetricsLogger::OnContextualSearchboxQueryIssued(
    bool is_zero_prefix_suggestion,
    bool is_initial_query) {
  csb_session_end_metrics_.zps_used_ =
      csb_session_end_metrics_.zps_used_ || is_zero_prefix_suggestion;
  csb_session_end_metrics_.query_issued_ = true;
  const bool is_follow_up_query = !is_initial_query;
  if (is_follow_up_query) {
    csb_session_end_metrics_.follow_up_query_issued_ = true;
  }
  if (is_initial_query &&
      !csb_session_end_metrics_.zps_shown_on_initial_query_) {
    // If the query was made in the initial state, and the ZPS has not been
    // shown, mark the query as issued before ZPS shown.
    csb_session_end_metrics_.initial_query_issued_before_zps_shown_ = true;
  } else if (is_follow_up_query &&
             !csb_session_end_metrics_.zps_shown_on_follow_up_query_) {
    // If a follow up query was made, and the ZPS has not been
    // shown for the follow up query, mark the query as issued before ZPS
    // shown.
    csb_session_end_metrics_.follow_up_query_issued_before_zps_shown_ = true;
  }

  // After the searchbox request is sent, mark the follow up zps as not shown so
  // it is false for the next follow up query.
  csb_session_end_metrics_.zps_shown_on_follow_up_query_ = false;
}

void LensSessionMetricsLogger::OnSearchboxFocused() {
  if (!csb_session_end_metrics_.searchbox_focused_) {
    // This is the first time the searchbox is focused in this session.
    // Record the time between the overlay being invoked and the searchbox
    // being focused.
    lens::RecordContextualSearchboxTimeToFirstFocus(
        base::TimeTicks::Now() - invocation_time_, initial_page_content_type_);
  } else {
    RecordContextualSearchboxTimeToFocusAfterNavigation();
  }
  csb_session_end_metrics_.searchbox_focused_ = true;
}

void LensSessionMetricsLogger::OnZeroSuggestShown(bool is_initial_query) {
  if (is_initial_query) {
    csb_session_end_metrics_.zps_shown_on_initial_query_ = true;
  } else {
    csb_session_end_metrics_.zps_shown_on_follow_up_query_ = true;
  }
}

void LensSessionMetricsLogger::RecordInvocation() {
  lens::RecordInvocation(invocation_source_, initial_document_type_);
}

void LensSessionMetricsLogger::RecordEndOfSessionMetrics(
    LensOverlayDismissalSource dismissal_source) {
  // UMA unsliced Dismissed.
  lens::RecordDismissal(dismissal_source);

  // UMA InvocationResultedInSearch.
  lens::RecordInvocationResultedInSearch(invocation_source_,
                                         search_performed_in_session_);

  // UMA session duration.
  DCHECK(!invocation_time_.is_null());
  base::TimeDelta session_duration = base::TimeTicks::Now() - invocation_time_;
  lens::RecordSessionDuration(invocation_source_, session_duration);

  // UKM session end metrics. Includes invocation source, whether the
  // session resulted in a search, invocation document type and session
  // duration.
  lens::RecordUKMSessionEndMetrics(ukm_source_id_, invocation_source_,
                                   search_performed_in_session_,
                                   session_duration, initial_document_type_);

  // UMA and UKM end of session metrics for the CSB. Only recorded if CSB is
  // shown in session.
  lens::RecordContextualSearchboxSessionEndMetrics(
      ukm_source_id_, csb_session_end_metrics_, initial_page_content_type_,
      initial_document_type_);
}

void LensSessionMetricsLogger::RecordTimeToFirstInteraction(
    lens::LensOverlayFirstInteractionType interaction_type) {
  if (search_performed_in_session_) {
    return;
  }
  DCHECK(!invocation_time_.is_null());
  base::TimeDelta time_to_first_interaction =
      base::TimeTicks::Now() - invocation_time_;
  // UMA and UKM TimeToFirstInteraction.
  lens::RecordTimeToFirstInteraction(invocation_source_,
                                     time_to_first_interaction,
                                     interaction_type, ukm_source_id_);
  search_performed_in_session_ = true;
}

void LensSessionMetricsLogger::
    RecordContextualSearchboxTimeToFocusAfterNavigation() {
  if (!last_navigation_time_.has_value() ||
      contextual_searchbox_focused_after_navigation_) {
    return;
  }
  base::TimeDelta time_to_focus =
      base::TimeTicks::Now() - last_navigation_time_.value();
  lens::RecordContextualSearchboxTimeToFocusAfterNavigation(
      time_to_focus, current_page_content_type_);
  contextual_searchbox_focused_after_navigation_ = true;
}

void LensSessionMetricsLogger::
    RecordContextualSearchboxTimeToInteractionAfterNavigation() {
  if (!last_navigation_time_.has_value()) {
    return;
  }
  base::TimeDelta time_to_interaction =
      base::TimeTicks::Now() - last_navigation_time_.value();
  lens::RecordContextualSearchboxTimeToInteractionAfterNavigation(
      time_to_interaction, current_page_content_type_);
  last_navigation_time_.reset();
}

}  // namespace lens

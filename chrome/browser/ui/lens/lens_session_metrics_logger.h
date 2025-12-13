// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SESSION_METRICS_LOGGER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SESSION_METRICS_LOGGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_mime_type.h"

namespace content {
class WebContents;
}

namespace lens {

struct ContextualSearchboxSessionEndMetrics;

// This class is responsible for tracking session-specific metrics for the Lens
// overlay and logging them at appropriate times, primarily at the end of a
// session.
class LensSessionMetricsLogger {
 public:
  LensSessionMetricsLogger();
  ~LensSessionMetricsLogger();

  LensSessionMetricsLogger(const LensSessionMetricsLogger&) = delete;
  LensSessionMetricsLogger& operator=(const LensSessionMetricsLogger&) = delete;

  // Call when a new Lens overlay session starts.
  void OnSessionStart(LensOverlayInvocationSource invocation_source,
                      content::WebContents* tab_web_contents);

  // Call to update navigation metrics.
  void OnPageNavigation();

  // Call when a search is performed within this session.
  void OnSearchPerformed();

  // Stores the type of the page content extracted from the page on invocation.
  void OnInitialPageContentRetrieved(lens::MimeType page_content_type);

  // Stores the type of the page content extracted from the page on follow up
  // page content retrieval.
  void OnFollowUpPageContentRetrieved(lens::MimeType page_content_type);

  // Called when the contextual searchbox is shown.
  void OnContextualSearchboxShown();

  // Called when a contextual searchbox query is issued.
  // `is_zero_prefix_suggestion` is true if the query was issued from a user
  // selecting a zero prefix suggestion. `is_initial_query` is true if the query
  // was issued from the initial state of the lens overlay, aka not from the
  // side panel.
  void OnContextualSearchboxQueryIssued(bool is_zero_prefix_suggestion,
                                        bool is_initial_query);

  // Called when the searchbox is focused. Used to log metrics relating to focus
  // events of the CSB.
  void OnSearchboxFocused();

  // Called when the zero prefix suggestions are shown to the user.
  void OnZeroSuggestShown(bool is_initial_query);

  // Called when the AIM composebox is shown in the side panel.
  void OnAimComposeboxShown();

  // Called when the AIM handshake is received.
  void OnAimHandshakeCompleted();

  // Called when the AIM composebox is focused.
  void OnAimComposeboxFocused();

  // Called when a query is issued in the AIM searchbox.
  void OnAimQueryIssued();

  // Called when a query is submitted in the AIM searchbox.
  void OnAimQuerySubmitted();

  // Records Lens invocation.
  void RecordInvocation();

  // Records UMA and UKM metrics for time to first interaction. Not recorded
  // when invocation source is an image's content area menu because in this
  // case the time to first interaction is essentially zero.
  void RecordTimeToFirstInteraction(
      lens::LensOverlayFirstInteractionType interaction_type);

  // Records UMA and UKM metrics for dismissal and end of session metrics.
  // This includes dismissal source, session length, and whether a search was
  // recorded in the session.
  void RecordEndOfSessionMetrics(
      lens::LensOverlayDismissalSource dismissal_source);

  // Records the UMA for the first time the contextual searchbox is focused
  // after the page has been navigated.
  void RecordContextualSearchboxTimeToFocusAfterNavigation();

  // Records the UMA for the first time the user interacts with the contextual
  // searchbox after the page has been navigated.
  void RecordContextualSearchboxTimeToInteractionAfterNavigation();

  // Returns the time at which the overlay was invoked.
  void GetInvocationTime();

  // Returns the invocation source for the lens session.
  lens::LensOverlayInvocationSource GetInvocationSource();

 private:
  // Invocation source for the lens overlay.
  lens::LensOverlayInvocationSource invocation_source_ =
      lens::LensOverlayInvocationSource::kAppMenu;

  // The time at which the overlay was invoked. Used to compute timing metrics.
  // Assumed to be when OnSessionStart is called.
  base::TimeTicks invocation_time_;

  // The time at which the live page navigated while in the contextual searchbox
  // flow. Used to compute timing metrics. Is empty if the user is not in the
  // contextual searchbox flow, or this navigation has already been recorded.
  std::optional<base::TimeTicks> last_navigation_time_;

  // Whether the contextual searchbox has been focused since the last page
  // navigation.
  bool contextual_searchbox_focused_after_navigation_ = false;

  // Indicates whether a search has been performed in the current session. Used
  // to record success/abandonment rate, as defined by whether or not a search
  // was performed.
  bool search_performed_in_session_ = false;

  // The UKM source id of the tab Lens was invoked on.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // The type of the page content extracted from the page when the lens overlay
  // was initialized. This is used when recording contextual searchbox metrics
  // at the end of sessions, since the initialization data can change on page
  // contextualization updates and these metrics only want to record the initial
  // invocation page content type.
  lens::MimeType initial_page_content_type_ = lens::MimeType::kUnknown;

  // The type of the document that the lens overlay was initialized on as
  // determined by the mime type reported by the tab web contents. This differs
  // from initial_page_content_type_ in that the document type is the type of
  // the top level document, while the intial_page_content_type_ is the type of
  // the content extracted from the page that we are contextualizing to. This is
  // used when recording invocation document metrics, since the document type
  // can change on page contextualization updates.
  lens::MimeType initial_document_type_ = lens::MimeType::kUnknown;

  // The type of the page content extracted from the page currently. Is kept up
  // to date as the page content changes.
  lens::MimeType current_page_content_type_ = lens::MimeType::kUnknown;

  // Metrics for the contextual searchbox that will be recorded at the end of a
  // session.
  ContextualSearchboxSessionEndMetrics csb_session_end_metrics_;

  // Metrics for the AIM searchbox that will be recorded at the end of a
  // session.
  AimSessionEndMetrics aim_session_end_metrics_;

  // Must be the last member.
  base::WeakPtrFactory<LensSessionMetricsLogger> weak_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SESSION_METRICS_LOGGER_H_

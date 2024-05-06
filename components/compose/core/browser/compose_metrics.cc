// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace compose {

const char kComposeDialogInnerTextShortenedBy[] =
    "Compose.Dialog.InnerTextShortenedBy";
const char kComposeDialogInnerTextSize[] = "Compose.Dialog.InnerTextSize";
const char kComposeDialogOpenLatency[] = "Compose.Dialog.OpenLatency";
const char kComposeDialogSelectionLength[] = "Compose.Dialog.SelectionLength";
const char kComposeRequestReason[] = "Compose.Request.Reason";
const char kComposeRequestDurationOkSuffix[] = ".Request.Duration.Ok";
const char kComposeRequestDurationErrorSuffix[] = ".Request.Duration.Error";
const char kComposeRequestStatus[] = "Compose.Request.Status";
const char kComposeSessionComposeCount[] = "Compose.Session.ComposeCount";
const char kComposeSessionCloseReason[] = "Compose.Session.CloseReason";
const char kComposeSessionDialogShownCount[] =
    "Compose.Session.DialogShownCount";
const char kComposeSessionEventCounts[] = "Compose.Session.EventCounts";
const char kComposeSessionDuration[] = "Compose.Session.Duration";
const char kComposeSessionOverOneDay[] = "Compose.Session.Duration.OverOneDay";
const char kComposeSessionUndoCount[] = "Compose.Session.UndoCount";
const char kComposeSessionUpdateInputCount[] =
    "Compose.Session.SubmitEditCount";
const char kComposeShowStatus[] = "Compose.ContextMenu.ShowStatus";
const char kComposeMSBBSessionCloseReason[] =
    "Compose.Session.FRE.MSBB.CloseReason";
const char kComposeMSBBSessionDialogShownCount[] =
    "Compose.Session.FRE.MSBB.DialogShownCount";
const char kComposeFirstRunSessionCloseReason[] =
    "Compose.Session.FRE.Disclaimer.CloseReason";
const char kComposeFirstRunSessionDialogShownCount[] =
    "Compose.Session.FRE.Disclaimer.DialogShownCount";
const char kInnerTextNodeOffsetFound[] =
    "Compose.Dialog.InnerTextNodeOffsetFound";
const char kComposeContextMenuCtr[] = "Compose.ContextMenu.CTR";
const char kComposeProactiveNudgeCtr[] = "Compose.ProactiveNudge.CTR";
const char kComposeProactiveNudgeShowStatus[] =
    "Compose.ProactiveNudge.ShowStatus";
const char kOpenComposeDialogResult[] =
    "Compose.ContextMenu.OpenComposeDialogResult";
const char kComposeSelectAll[] = "Compose.ContextMenu.SelectedAll";

namespace {

std::string_view EvalLocationString(EvalLocation location) {
  switch (location) {
    case compose::EvalLocation::kServer:
      return "Server";
    case EvalLocation::kOnDevice:
      return "OnDevice";
  }
}

// Emit an enum for for each event present in `session_events`.
// Split the event counts histogram on `eval_location` if provided.
void LogComposeSessionEventCounts(std::optional<EvalLocation> eval_location,
                                  const ComposeSessionEvents& session_events) {
  std::string histogram;
  if (!eval_location) {
    histogram = kComposeSessionEventCounts;
  } else {
    histogram = base::StrCat({"Compose.", EvalLocationString(*eval_location),
                              ".Session.EventCounts"});
  }
  if (session_events.dialog_shown_count > 0) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kDialogShown);
  }
  if (session_events.fre_dialog_shown_count > 0) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kFREShown);
  }
  if (session_events.fre_completed_in_session) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kFREAccepted);
  }
  if (session_events.msbb_dialog_shown_count > 0) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kMSBBShown);
  }
  if (session_events.msbb_settings_opened) {
    base::UmaHistogramEnumeration(
        histogram, ComposeSessionEventTypes::kMSBBSettingsOpened);
  }
  if (session_events.msbb_enabled_in_session) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kMSBBEnabled);
  }
  if (session_events.has_initial_text) {
    base::UmaHistogramEnumeration(
        histogram, ComposeSessionEventTypes::kStartedWithSelection);
  }
  if (session_events.compose_count > 0) {
    // The first Compose event has to be "Create".
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kCreateClicked);
  }
  if (session_events.update_input_count > 0) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kUpdateClicked);
  }
  if (session_events.regenerate_count > 0) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kRetryClicked);
  }
  if (session_events.undo_count > 0) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kUndoClicked);
  }
  if (session_events.redo_count > 0) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kRedoClicked);
  }
  if (session_events.result_edit_count > 0) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kResultEdited);
  }
  bool has_used_modifier = false;
  if (session_events.shorten_count > 0) {
    has_used_modifier = true;
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kShortenClicked);
  }
  if (session_events.lengthen_count > 0) {
    has_used_modifier = true;
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kElaborateClicked);
  }
  if (session_events.casual_count > 0) {
    has_used_modifier = true;
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kCasualClicked);
  }
  if (session_events.formal_count > 0) {
    has_used_modifier = true;
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kFormalClicked);
  }
  if (has_used_modifier) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kAnyModifierUsed);
  }
  if (session_events.has_thumbs_down) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kThumbsDown);
  }
  if (session_events.has_thumbs_up) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kThumbsUp);
  }
  if (session_events.inserted_results) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kInsertClicked);
  }
  if (session_events.edited_result_inserted) {
    base::UmaHistogramEnumeration(
        histogram, ComposeSessionEventTypes::kEditedResultInserted);
  }
  if (session_events.close_clicked) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kCloseClicked);
  }
  if (session_events.did_click_edit) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kEditClicked);
  }
  if (session_events.did_click_cancel_on_edit) {
    base::UmaHistogramEnumeration(histogram,
                                  ComposeSessionEventTypes::kCancelEditClicked);
  }
}

}  // namespace

PageUkmTracker::PageUkmTracker(ukm::SourceId source_id)
    : source_id(source_id) {}

PageUkmTracker::~PageUkmTracker() {
  MaybeLogUkm();
}

void PageUkmTracker::MenuItemShown() {
  event_was_recorded_ = true;
  ++menu_item_shown_count_;
}

void PageUkmTracker::MenuItemClicked() {
  event_was_recorded_ = true;
  ++menu_item_clicked_count_;
}

void PageUkmTracker::ComposeTextInserted() {
  event_was_recorded_ = true;
  ++compose_text_inserted_count_;
}

void PageUkmTracker::ComposeProactiveNudgeShouldShow() {
  event_was_recorded_ = true;
  ++compose_proactive_nudge_should_show_;
}

void PageUkmTracker::ShowDialogAbortedDueToMissingFormData() {
  event_was_recorded_ = true;
  ++missing_form_data_count_;
}

void PageUkmTracker::ShowDialogAbortedDueToMissingFormFieldData() {
  event_was_recorded_ = true;
  ++missing_form_field_data_count_;
}

void PageUkmTracker::MaybeLogUkm() {
  if (!event_was_recorded_) {
    return;
  }

  ukm::builders::Compose_PageEvents(source_id)
      .SetMenuItemShown(
          ukm::GetExponentialBucketMinForCounts1000(menu_item_shown_count_))
      .SetMenuItemClicked(
          ukm::GetExponentialBucketMinForCounts1000(menu_item_clicked_count_))
      .SetComposeTextInserted(ukm::GetExponentialBucketMinForCounts1000(
          compose_text_inserted_count_))
      .SetProactiveNudgeShouldShow(ukm::GetExponentialBucketMinForCounts1000(
          compose_proactive_nudge_should_show_))
      .SetMissingFormData(
          ukm::GetExponentialBucketMinForCounts1000(missing_form_data_count_))
      .SetMissingFormFieldData(ukm::GetExponentialBucketMinForCounts1000(
          missing_form_field_data_count_))
      .Record(ukm::UkmRecorder::Get());
}

ComposeSessionEvents::ComposeSessionEvents() {}

void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event) {
  base::UmaHistogramEnumeration(kComposeContextMenuCtr, event);
}

void LogComposeContextMenuShowStatus(ComposeShowStatus status) {
  base::UmaHistogramEnumeration(kComposeShowStatus, status);
}

void LogComposeProactiveNudgeCtr(ComposeProactiveNudgeCtrEvent event) {
  base::UmaHistogramEnumeration(kComposeProactiveNudgeCtr, event);
}

void LogComposeProactiveNudgeShowStatus(ComposeShowStatus status) {
  base::UmaHistogramEnumeration(kComposeProactiveNudgeShowStatus, status);
}

void LogOpenComposeDialogResult(OpenComposeDialogResult result) {
  base::UmaHistogramEnumeration(kOpenComposeDialogResult, result);
}

void LogComposeRequestReason(ComposeRequestReason reason) {
  base::UmaHistogramEnumeration(kComposeRequestReason, reason);
}

void LogComposeRequestReason(EvalLocation eval_location,
                             ComposeRequestReason reason) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Compose.", EvalLocationString(eval_location), ".Request.Reason"}),
      reason);
}

void LogComposeRequestStatus(compose::mojom::ComposeStatus status) {
  base::UmaHistogramEnumeration(kComposeRequestStatus, status);
}

void LogComposeRequestStatus(EvalLocation eval_location,
                             compose::mojom::ComposeStatus status) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Compose.", EvalLocationString(eval_location), ".Request.Status"}),
      status);
}

void LogComposeRequestDuration(base::TimeDelta duration,
                               EvalLocation eval_location,
                               bool is_ok) {
  std::string_view suffix =
      is_ok ? std::string_view(kComposeRequestDurationOkSuffix)
            : std::string_view(kComposeRequestDurationErrorSuffix);
  base::UmaHistogramMediumTimes(base::StrCat({"Compose", suffix}), duration);
  base::UmaHistogramMediumTimes(
      base::StrCat({"Compose.", EvalLocationString(eval_location), suffix}),
      duration);
}

void LogComposeFirstRunSessionCloseReason(
    ComposeFirstRunSessionCloseReason reason) {
  base::UmaHistogramEnumeration(kComposeFirstRunSessionCloseReason, reason);
}

void LogComposeFirstRunSessionDialogShownCount(
    ComposeFirstRunSessionCloseReason reason,
    int dialog_shown_count) {
  std::string status;
  switch (reason) {
    case ComposeFirstRunSessionCloseReason::
        kFirstRunDisclaimerAcknowledgedWithoutInsert:
    case ComposeFirstRunSessionCloseReason::
        kFirstRunDisclaimerAcknowledgedWithInsert:
      status = ".Acknowledged";
      break;
    case ComposeFirstRunSessionCloseReason::kCloseButtonPressed:
    case ComposeFirstRunSessionCloseReason::kEndedImplicitly:
    case ComposeFirstRunSessionCloseReason::kNewSessionWithSelectedText:
      status = ".Ignored";
  }
  base::UmaHistogramCounts1000(kComposeFirstRunSessionDialogShownCount + status,
                               dialog_shown_count);
}

void LogComposeMSBBSessionCloseReason(ComposeMSBBSessionCloseReason reason) {
  base::UmaHistogramEnumeration(kComposeMSBBSessionCloseReason, reason);
}

void LogComposeMSBBSessionDialogShownCount(ComposeMSBBSessionCloseReason reason,
                                           int dialog_shown_count) {
  std::string status;
  switch (reason) {
    case ComposeMSBBSessionCloseReason::kMSBBAcceptedWithoutInsert:
    case ComposeMSBBSessionCloseReason::kMSBBAcceptedWithInsert:
      status = ".Accepted";
      break;
    case ComposeMSBBSessionCloseReason::kMSBBEndedImplicitly:
    case ComposeMSBBSessionCloseReason::kMSBBCloseButtonPressed:
      status = ".Ignored";
  }
  base::UmaHistogramCounts1000(kComposeMSBBSessionDialogShownCount + status,
                               dialog_shown_count);
}

SessionEvalLocation GetSessionEvalLocationFromEvents(
    const ComposeSessionEvents& session_events) {
  if (session_events.server_responses == 0 &&
      session_events.on_device_responses == 0) {
    return SessionEvalLocation::kNone;
  } else if (session_events.server_responses > 0 &&
             session_events.on_device_responses > 0) {
    return SessionEvalLocation::kMixed;
  } else if (session_events.server_responses > 0) {
    return SessionEvalLocation::kServer;
  } else {
    return SessionEvalLocation::kOnDevice;
  }
}

std::optional<EvalLocation> GetEvalLocationFromEvents(
    const ComposeSessionEvents& session_events) {
  switch (GetSessionEvalLocationFromEvents(session_events)) {
    case SessionEvalLocation::kNone:
    case SessionEvalLocation::kMixed:
      return std::nullopt;
    case SessionEvalLocation::kServer:
      return EvalLocation::kServer;
    case SessionEvalLocation::kOnDevice:
      return EvalLocation::kOnDevice;
  }
}

void LogComposeSessionCloseMetrics(ComposeSessionCloseReason reason,
                                   const ComposeSessionEvents& session_events) {
  std::string status;
  switch (reason) {
    case ComposeSessionCloseReason::kAcceptedSuggestion:
      status = ".Accepted";
      break;
    case ComposeSessionCloseReason::kCloseButtonPressed:
    case ComposeSessionCloseReason::kEndedImplicitly:
    case ComposeSessionCloseReason::kNewSessionWithSelectedText:
    case ComposeSessionCloseReason::kCanceledBeforeResponseReceived:
      status = ".Ignored";
  }

  // Report all session-agnostic metrics.
  base::UmaHistogramEnumeration(kComposeSessionCloseReason, reason);
  base::UmaHistogramCounts1000(kComposeSessionComposeCount + status,
                               session_events.compose_count);
  base::UmaHistogramCounts1000(kComposeSessionDialogShownCount + status,
                               session_events.dialog_shown_count);
  base::UmaHistogramCounts1000(kComposeSessionUndoCount + status,
                               session_events.undo_count);
  base::UmaHistogramCounts1000(kComposeSessionUpdateInputCount + status,
                               session_events.update_input_count);
  LogComposeSessionEventCounts(std::nullopt, session_events);

  // Report all eval location specific metrics.
  SessionEvalLocation session_eval_location =
      GetSessionEvalLocationFromEvents(session_events);
  std::optional<EvalLocation> eval_location =
      GetEvalLocationFromEvents(session_events);
  base::UmaHistogramEnumeration("Compose.Session.EvalLocation",
                                session_eval_location);
  if (eval_location) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Compose.", EvalLocationString(*eval_location),
                      ".Session.CloseReason"}),
        reason);
    base::UmaHistogramCounts1000(
        base::StrCat({"Compose.", EvalLocationString(*eval_location),
                      ".Session.ComposeCount", status}),
        session_events.compose_count);
    base::UmaHistogramCounts1000(
        base::StrCat({"Compose.", EvalLocationString(*eval_location),
                      ".Session.DialogShownCount", status}),
        session_events.dialog_shown_count);
    base::UmaHistogramCounts1000(
        base::StrCat({"Compose.", EvalLocationString(*eval_location),
                      ".Session.UndoCount", status}),
        session_events.undo_count);
    base::UmaHistogramCounts1000(
        base::StrCat({"Compose.", EvalLocationString(*eval_location),
                      ".Session.SubmitEditCount", status}),
        session_events.update_input_count);
    LogComposeSessionEventCounts(eval_location, session_events);
  }
}

void LogComposeSessionCloseUkmMetrics(
    ukm::SourceId source_id,
    const ComposeSessionEvents& session_events) {
  // Log the UKM metrics for this session.
  ukm::builders::Compose_SessionProgress(source_id)
      .SetDialogShownCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.dialog_shown_count))
      .SetComposeCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.compose_count))
      .SetShortenCount(session_events.shorten_count)
      .SetLengthenCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.lengthen_count))
      .SetFormalCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.formal_count))
      .SetCasualCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.casual_count))
      .SetRegenerateCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.regenerate_count))
      .SetUndoCount(
          ukm::GetExponentialBucketMinForCounts1000(session_events.undo_count))
      .SetInsertedResults(session_events.inserted_results)
      .SetCanceled(session_events.close_clicked)
      .Record(ukm::UkmRecorder::Get());
}

void LogComposeDialogInnerTextShortenedBy(int shortened_by) {
  base::UmaHistogramCounts10M(kComposeDialogInnerTextShortenedBy, shortened_by);
}

void LogComposeDialogInnerTextSize(int size) {
  base::UmaHistogramCounts10M(kComposeDialogInnerTextSize, size);
}

void LogComposeDialogInnerTextOffsetFound(bool inner_offset_found) {
  UMA_HISTOGRAM_ENUMERATION(kInnerTextNodeOffsetFound,
                            inner_offset_found
                                ? ComposeInnerTextNodeOffset::kOffsetFound
                                : ComposeInnerTextNodeOffset::kNoOffsetFound);
}

void LogComposeDialogOpenLatency(base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(kComposeDialogOpenLatency, duration);
}

void LogComposeDialogSelectionLength(int length) {
  // The autofill::kMaxSelectedTextLength is in UTF16 bytes so divide by 2 for
  // the maximum number of unicode code points.
  const int max_selection_size = 51200 / 2;
  base::UmaHistogramCustomCounts(kComposeDialogSelectionLength, length, 1,
                                 max_selection_size + 1, 100);
}

void LogComposeSessionDuration(base::TimeDelta session_duration,
                               std::string session_suffix,
                               std::optional<EvalLocation> eval_location) {
  base::UmaHistogramLongTimes100(kComposeSessionDuration + session_suffix,
                                 session_duration);
  if (eval_location) {
    base::UmaHistogramLongTimes100(
        base::StrCat({"Compose.", EvalLocationString(*eval_location),
                      ".Session.Duration", session_suffix}),
        session_duration);
  }

  if (session_duration.InDays() > 1) {
    base::UmaHistogramBoolean(kComposeSessionOverOneDay, true);
  } else {
    base::UmaHistogramBoolean(kComposeSessionOverOneDay, false);
  }
}

void LogComposeRequestFeedback(EvalLocation eval_location,
                               ComposeRequestFeedback feedback) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Compose.", EvalLocationString(eval_location), ".Request.Feedback"}),
      feedback);
}

void LogComposeSelectAllStatus(ComposeSelectAllStatus select_all_status) {
  base::UmaHistogramEnumeration(kComposeSelectAll, select_all_status);
}

}  // namespace compose

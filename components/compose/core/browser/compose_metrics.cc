// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
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
const char kComposeResponseDurationOk[] = "Compose.Response.Duration.Ok";
const char kComposeResponseDurationError[] = "Compose.Response.Duration.Error";
const char kComposeResponseStatus[] = "Compose.Response.Status";
const char kComposeSessionComposeCount[] = "Compose.Session.ComposeCount";
const char kComposeSessionCloseReason[] = "Compose.Session.CloseReason";
const char kComposeSessionDialogShownCount[] =
    "Compose.Session.DialogShownCount";
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
      .Record(ukm::UkmRecorder::Get());
}

void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Compose.ContextMenu.CTR", event);
}

void LogComposeContextMenuShowStatus(ComposeShowStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kComposeShowStatus, status);
}

void LogComposeRequestReason(ComposeRequestReason reason) {
  UMA_HISTOGRAM_ENUMERATION(kComposeRequestReason, reason);
}

void LogComposeRequestDuration(base::TimeDelta duration, bool is_valid) {
  base::UmaHistogramMediumTimes(
      is_valid ? kComposeResponseDurationOk : kComposeResponseDurationError,
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

void LogComposeSessionCloseMetrics(ComposeSessionCloseReason reason,
                                   ComposeSessionEvents session_events) {
  base::UmaHistogramEnumeration(kComposeSessionCloseReason, reason);

  std::string status;
  switch (reason) {
    case ComposeSessionCloseReason::kAcceptedSuggestion:
      status = ".Accepted";
      break;
    case ComposeSessionCloseReason::kCloseButtonPressed:
    case ComposeSessionCloseReason::kEndedImplicitly:
    case ComposeSessionCloseReason::kNewSessionWithSelectedText:
      status = ".Ignored";
  }
  base::UmaHistogramCounts1000(kComposeSessionComposeCount + status,
                               session_events.compose_count);
  base::UmaHistogramCounts1000(kComposeSessionDialogShownCount + status,
                               session_events.dialog_shown_count);
  base::UmaHistogramCounts1000(kComposeSessionUndoCount + status,
                               session_events.undo_count);
  base::UmaHistogramCounts1000(kComposeSessionUpdateInputCount + status,
                               session_events.update_input_count);
}

void LogComposeDialogInnerTextShortenedBy(int shortened_by) {
  base::UmaHistogramCounts10M(kComposeDialogInnerTextShortenedBy, shortened_by);
}

void LogComposeDialogInnerTextSize(int size) {
  base::UmaHistogramCounts10M(kComposeDialogInnerTextSize, size);
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

}  // namespace compose

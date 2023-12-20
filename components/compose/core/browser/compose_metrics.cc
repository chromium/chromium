// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

namespace compose {

const char kComposeDialogInnerTextShortenedBy[] =
    "Compose.Dialog.InnerTextShortenedBy";
const char kComposeDialogInnerTextSize[] = "Compose.Dialog.InnerTextSize";
const char kComposeDialogOpenLatency[] = "Compose.Dialog.OpenLatency";
const char kComposeDialogSelectionLength[] = "Compose.Dialog.SelectionLength";
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
const char kComposeConsentSessionCloseReason[] =
    "Compose.Session.Consent.CloseReason";
const char kComposeConsentSessionDialogShownCount[] =
    "Compose.Session.Consent.DialogShownCount";
const char kComposeMSBBSessionCloseReason[] =
    "Compose.Session.FRE.MSBB.CloseReason";
const char kComposeMSBBSessionDialogShownCount[] =
    "Compose.Session.FRE.MSBB.DialogShownCount";
const char kComposeSessionConsentGivenInSession[] =
    "Compose.Session.Consent.GivenInSession";
const char kComposeFirstRunSessionCloseReason[] =
    "Compose.Session.FRE.Disclaimer.CloseReason";
const char kComposeFirstRunSessionDialogShownCount[] =
    "Compose.Session.FRE.Disclaimer.DialogShownCount";

void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Compose.ContextMenu.CTR", event);
}

void LogComposeContextMenuShowStatus(ComposeShowStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kComposeShowStatus, status);
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

void LogComposeConsentSessionCloseReason(
    ComposeConsentSessionCloseReason reason) {
  base::UmaHistogramEnumeration(kComposeConsentSessionCloseReason, reason);
}

void LogComposeMSBBSessionCloseReason(ComposeMSBBSessionCloseReason reason) {
  base::UmaHistogramEnumeration(kComposeMSBBSessionCloseReason, reason);
}

void LogComposeConsentSessionDialogShownCount(
    ComposeConsentSessionCloseReason reason,
    int dialog_shown_count) {
  std::string status;
  switch (reason) {
    case ComposeConsentSessionCloseReason::
        kPageContentConsentAcceptedWithoutInsert:
    case ComposeConsentSessionCloseReason::
        kPageContentDisclaimerAcknowledgedWithoutInsert:
    case ComposeConsentSessionCloseReason::kPageContentConsentGivenWithInsert:
      status = ".Accepted";
      break;
    case ComposeConsentSessionCloseReason::kCloseButtonPressed:
    case ComposeConsentSessionCloseReason::kPageContentConsentDeclined:
    case compose::ComposeConsentSessionCloseReason::kEndedImplicitly:
    case ComposeConsentSessionCloseReason::kNewSessionWithSelectedText:
      status = ".Ignored";
  }
  base::UmaHistogramCounts1000(kComposeConsentSessionDialogShownCount + status,
                               dialog_shown_count);
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
                                   int compose_count,
                                   int dialog_shown_count,
                                   int undo_count,
                                   int update_input_count_,
                                   bool consent_given_in_session) {
  base::UmaHistogramEnumeration(kComposeSessionCloseReason, reason);
  base::UmaHistogramBoolean(kComposeSessionConsentGivenInSession,
                            consent_given_in_session);

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
                               compose_count);
  base::UmaHistogramCounts1000(kComposeSessionDialogShownCount + status,
                               dialog_shown_count);
  base::UmaHistogramCounts1000(kComposeSessionUndoCount + status, undo_count);
  base::UmaHistogramCounts1000(kComposeSessionUpdateInputCount + status,
                               update_input_count_);
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

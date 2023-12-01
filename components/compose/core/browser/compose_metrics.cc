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
const char kComposeShowStatus[] = "Compose.ContextMenu.ShowStatus";

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

void LogComposeSessionCloseMetrics(ComposeSessionCloseReason reason,
                                   int compose_count,
                                   int dialog_shown_count,
                                   int undo_count) {
  UMA_HISTOGRAM_ENUMERATION(kComposeSessionCloseReason, reason);

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

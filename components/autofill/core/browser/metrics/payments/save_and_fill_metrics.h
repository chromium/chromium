// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_AND_FILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_AND_FILL_METRICS_H_

#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SaveAndFillFormEvent)
enum class SaveAndFillFormEvent {
  // The Save and Fill suggestion was shown to the user.
  kSuggestionShown = 0,
  // The Save and Fill suggestion was accepted by the user.
  kSuggestionAccepted = 1,
  // The form was filled after Save and Fill finished.
  kFormFilled = 2,
  // The form was submitted after Save and Fill finished.
  kFormSubmitted = 3,
  kMaxValue = kFormSubmitted,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:SaveAndFillFormEvent)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SaveAndFillSuggestionNotShownReason)
enum class SaveAndFillSuggestionNotShownReason {
  // The user has at least one credit card saved.
  kHasSavedCards = 0,
  // The suggestion is blocked by the strike database (e.g., max strikes
  // reached or required delay has not passed).
  kBlockedByStrikeDatabase = 1,
  // The user is in incognito mode.
  kUserInIncognito = 2,
  // The credit card form is not complete.
  kIncompleteCreditCardForm = 3,
  kMaxValue = kIncompleteCreditCardForm,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:SaveAndFillSuggestionNotShownReason)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SaveAndFillDialogResult)
enum class SaveAndFillDialogResult {
  // User accepted the dialog and provided a CVC.
  kAcceptedWithCvc = 0,
  // User accepted the dialog but did not provide a CVC.
  kAcceptedWithoutCvc = 1,
  // User declined the dialog.
  kCanceled = 2,
  kMaxValue = kCanceled,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:SaveAndFillDialogResult)

void LogSaveAndFillFormEvent(SaveAndFillFormEvent event);

// Logs the reason why the Save and Fill suggestion was not shown.
void LogSaveAndFillSuggestionNotShownReason(
    SaveAndFillSuggestionNotShownReason reason);

// Logs the latency for the GetDetailsForCreateCard & CreateCard request. Logs
// to parent histogram with no breakdown by result and child histograms with
// specific result (success/failure) of the request. This is due to the latency
// of the failed requests having a larger variation and possible long tails.
void LogSaveAndFillGetDetailsForCreateCardResultAndLatency(
    bool succeeded,
    base::TimeDelta latency);
void LogSaveAndFillCreateCardResultAndLatency(bool succeeded,
                                              base::TimeDelta latency);

void LogSaveAndFillStrikeDatabaseBlockReason(
    AutofillMetrics::AutofillStrikeDatabaseBlockReason reason);
void LogSaveAndFillNumOfStrikesPresentWhenDialogAccepted(int strike_count);

// Logs the result of the Save and Fill dialog.
void LogSaveAndFillDialogResult(SaveAndFillDialogResult result);

// Logs that the Save and Fill dialog was shown.
void LogSaveAndFillDialogShown(bool is_upload);

// Logs the form being filled and form being submitted event. Broken down by
// whether the Save and Fill attempt succeeded and whether it was for upload
// Save and Fill.
void LogSaveAndFillFunnelMetrics(bool succeeded,
                                 bool is_for_upload,
                                 SaveAndFillFormEvent event);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_AND_FILL_METRICS_H_

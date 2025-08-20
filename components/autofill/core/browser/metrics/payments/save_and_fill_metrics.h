// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_AND_FILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_AND_FILL_METRICS_H_

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
  kMaxValue = kSuggestionAccepted,
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

void LogSaveAndFillFormEvent(SaveAndFillFormEvent event);

// Logs the reason why the Save and Fill suggestion was not shown.
void LogSaveAndFillSuggestionNotShownReason(
    SaveAndFillSuggestionNotShownReason reason);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_AND_FILL_METRICS_H_

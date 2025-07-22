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

void LogSaveAndFillFormEvent(SaveAndFillFormEvent event);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_SAVE_AND_FILL_METRICS_H_

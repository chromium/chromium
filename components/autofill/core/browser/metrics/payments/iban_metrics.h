// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_IBAN_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_IBAN_METRICS_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

// This includes all possible results.
// They will be used in metrics, and should not be renumbered.
enum class SaveIbanBubbleResult {
  // The user explicitly accepted the bubble by clicking the ok button.
  kAccepted = 0,
  // The user explicitly cancelled the bubble by clicking the cancel button.
  kCancelled = 1,
  // The user explicitly closed the bubble with the close button or ESC.
  kClosed = 2,
  // The user did not interact with the bubble.
  kNotInteracted = 3,
  // The bubble lost focus and was deactivated.
  kLostFocus = 4,
  // The reason why the bubble is closed is not clear. Possible reason is the
  // logging function is invoked before the closed reason is correctly set.
  kUnknown = 5,
  kMaxValue = kUnknown,
};

// Metrics to track event when the IBAN prompt is offered.
// They will be used in metrics, and should not be renumbered.
enum class SaveIbanPromptOffer {
  // The prompt is actually shown.
  kShown = 0,
  // The prompt is not shown because the prompt has been declined by the user
  // too many times.
  kNotShownMaxStrikesReached = 1,
  kMaxValue = kNotShownMaxStrikesReached,
};

// Metrics to track events related to individual IBAN suggestions in the
// IBANs suggestions popup.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IbanSuggestionsEvent {
  // IBAN suggestions were shown for a field. Logged one time for each time the
  // popup appeared, regardless of the number of suggestions shown.
  kIbanSuggestionsShown = 0,
  // IBAN suggestions were shown for a field. Logged only once per IBAN field.
  // It won't log more than once even if the user repeatedly displays
  // suggestions for the same field, or if the user alternates between this IBAN
  // field and the other non-IBAN fields.
  kIbanSuggestionsShownOnce = 1,
  // An individual IBAN suggestion was selected.
  kIbanSuggestionSelected = 2,
  // An individual IBAN suggestion was selected. Logged only once per IBAN
  // field. It won't log more than once if the user repeatedly selects IBAN
  // suggestion for the same field, or if the user alternates between this IBAN
  // field and the other non-IBAN fields and then click on IBAN suggestion.
  kIbanSuggestionSelectedOnce = 3,
  kMaxValue = kIbanSuggestionSelectedOnce,
};

// Logs various metrics about the local IBANs associated with a profile. This
// should be called each time a new Chrome profile is launched.
// `disused_data_threshold` is the time threshold to mark an IBAN as disused.
void LogStoredIbanMetrics(const std::vector<std::unique_ptr<IBAN>>& local_ibans,
                          const base::TimeDelta& disused_data_threshold);

// Logs the number of strikes that an IBAN had when save was accepted.
void LogStrikesPresentWhenIBANSaved(const int num_strikes);

// Logs whenever IBAN save is not offered due to max strikes.
void LogIBANSaveNotOfferedDueToMaxStrikesMetric(
    AutofillMetrics::SaveTypeMetric metric);

// Logs when IBAN save bubble is offered to users.
void LogSaveIbanBubbleOfferMetric(SaveIbanPromptOffer metric, bool is_reshow);

// Logs when the user makes a decision on the IBAN save bubble.
void LogSaveIbanBubbleResultMetric(SaveIbanBubbleResult metric, bool is_reshow);

// Logs when the user accepts the bubble to save an IBAN.
// `save_with_nickname` donates the user has input a nickname.
void LogSaveIbanBubbleResultSavedWithNicknameMetric(bool save_with_nickname);

// Logs metrics related to IBAN individual suggestions being shown or selected.
void LogIndividualIbanSuggestionsEvent(IbanSuggestionsEvent event);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_IBAN_METRICS_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CREDIT_CARD_SAVE_METRICS_DESKTOP_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CREDIT_CARD_SAVE_METRICS_DESKTOP_H_

#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"

namespace autofill::autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SaveCardPromptResultDesktop {
  // The user explicitly accepted the prompt by clicking the ok button.
  kAccepted = 0,
  // The user explicitly cancelled the prompt by clicking the cancel button.
  kCancelled = 1,
  // The user explicitly closed the prompt with the close button or ESC.
  kClosed = 2,
  // The user did not interact with the prompt.
  kNotInteracted = 3,
  // The prompt lost focus and was deactivated.
  kLostFocus = 4,
  // The reason why the prompt was closed is not clear. Possible reason is the
  // logging function is invoked before the closed reason is correctly set.
  kUnknown = 5,
  kMaxValue = kUnknown,
};

// Logs whether the save credit card prompt is shown or not on desktop. Should
// not be called for prompt re-shows (e.g., prompt reshown from the omnibox
// icon).
void LogSaveCreditCardPromptOfferMetricDesktop(
    SaveCardPromptOffer metric,
    bool is_upload_save,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions&
        save_credit_card_options);

// Logs metric capturing the user's interaction with the save credit card
// prompt. `has_saved_cards` indicates that local or server cards existed before
// the save prompt was accepted/denied. Should not be called for prompt re-shows
// (e.g., prompt reshown from the omnibox icon).
void LogSaveCreditCardPromptResultMetricDesktop(
    SaveCardPromptResultDesktop metric,
    bool is_upload_save,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions&
        save_credit_card_options,
    bool has_saved_cards);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CREDIT_CARD_SAVE_METRICS_DESKTOP_H_

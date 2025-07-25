// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CREDIT_CARD_SAVE_METRICS_DESKTOP_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CREDIT_CARD_SAVE_METRICS_DESKTOP_H_

#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"

namespace autofill::autofill_metrics {

// Logs whether the save credit card prompt is shown or not on desktop. Should
// not be called for prompt re-shows (e.g., prompt reshown from the omnibox
// icon).
void LogSaveCreditCardPromptOfferMetricDesktop(
    SaveCardPromptOffer metric,
    bool is_upload_save,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions&
        save_credit_card_options);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CREDIT_CARD_SAVE_METRICS_DESKTOP_H_

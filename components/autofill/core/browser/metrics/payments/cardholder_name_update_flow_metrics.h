// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARDHOLDER_NAME_UPDATE_FLOW_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARDHOLDER_NAME_UPDATE_FLOW_METRICS_H_

#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"

namespace autofill::autofill_metrics {

// Logs whether the cardholder name update prompt was shown. Not logged for
// reshows.
void LogCardholderNameUpdateFlowPromptOffer(SaveCardPromptOffer offer);

// Logs how the cardholder name update prompt was closed. Not logged for
// reshows.
void LogCardholderNameUpdateFlowPromptResult(
    PaymentsUiClosedReason closed_reason);

// Logs whether the user edited the prefilled cardholder name before accepting
// the cardholder name update prompt.
void LogCardholderNameUpdateFlowCardholderNameWasEdited(bool edited);

// Logs the result and the latency of the GetDetailsForUpdateCard request.
void LogGetDetailsForUpdateCardRequestResultAndLatency(
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    base::TimeDelta latency);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARDHOLDER_NAME_UPDATE_FLOW_METRICS_H_

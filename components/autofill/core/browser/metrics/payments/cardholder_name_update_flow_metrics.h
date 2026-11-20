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

// Eligibility of a form-extracted credit card for the cardholder name update
// flow.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(CardholderNameUpdateFlowEligibility)
enum class CardholderNameUpdateFlowEligibility {
  kEligible = 0,
  kServerCardAlreadyHasName = 1,
  kNoNameExtractedFromForm = 2,
  kInvalidNameExtractedFromForm = 3,
  kInvalidOrMissingCvcExtractedFromForm = 4,
  kSuppressedByVcnEnrollment = 5,
  kPreflightFailed = 6,
  kBlockedByStrikeDatabaseMaxStrikes = 7,
  kBlockedByStrikeDatabaseRequiredDelayNotPassed = 8,
  kMaxValue = kBlockedByStrikeDatabaseRequiredDelayNotPassed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillCardholderNameUpdateFlowEligibility)

// Logs the eligibility of a form-extracted credit card for the cardholder name
// update flow.
void LogCardholderNameUpdateFlowEligibility(
    CardholderNameUpdateFlowEligibility eligibility);

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

// Logs the result and the latency of the UpdateCard request.
void LogUpdateCardRequestResultAndLatency(
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    base::TimeDelta latency);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARDHOLDER_NAME_UPDATE_FLOW_METRICS_H_

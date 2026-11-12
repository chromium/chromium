// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/cardholder_name_update_flow_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/payments_rpc_metrics.h"

namespace autofill::autofill_metrics {

void LogCardholderNameUpdateFlowPromptOffer(SaveCardPromptOffer offer) {
  base::UmaHistogramEnumeration("Autofill.CardholderNameUpdateFlow.PromptOffer",
                                offer);
}

void LogCardholderNameUpdateFlowPromptResult(
    PaymentsUiClosedReason closed_reason) {
  base::UmaHistogramEnumeration(
      "Autofill.CardholderNameUpdateFlow.PromptResult", closed_reason);
}

void LogCardholderNameUpdateFlowCardholderNameWasEdited(bool edited) {
  base::UmaHistogramBoolean(
      "Autofill.CardholderNameUpdateFlow.CardholderNameWasEdited", edited);
}

void LogGetDetailsForUpdateCardRequestResultAndLatency(
    payments::PaymentsAutofillClient::PaymentsRpcResult result,
    base::TimeDelta latency) {
  base::UmaHistogramEnumeration(
      "Autofill.CardholderNameUpdateFlow.GetDetailsForUpdateCardRequest.Result",
      GetPaymentsRpcResultMetric(result));
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"Autofill.CardholderNameUpdateFlow.GetDetailsForUpdateCardRequest."
           "Latency",
           PaymentsRpcResultToMetricsSuffix(result)}),
      latency);
}

}  // namespace autofill::autofill_metrics

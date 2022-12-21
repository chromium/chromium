// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

void LogServerCardUnmaskAttempt(AutofillClient::PaymentsRpcCardType card_type) {
  base::UmaHistogramBoolean(
      "Autofill.ServerCardUnmask" +
          AutofillMetrics::GetHistogramStringForCardType(card_type) +
          ".Attempt",
      true);
}

void LogServerCardUnmaskResult(ServerCardUnmaskResult unmask_result,
                               AutofillClient::PaymentsRpcCardType card_type,
                               VirtualCardUnmaskFlowType flow_type) {
  std::string flow_type_suffix;
  switch (flow_type) {
    case VirtualCardUnmaskFlowType::kUnspecified:
      flow_type_suffix = ".UnspecifiedFlowType";
      break;
    case VirtualCardUnmaskFlowType::kFidoOnly:
      flow_type_suffix = ".Fido";
      break;
    case VirtualCardUnmaskFlowType::kOtpOnly:
      flow_type_suffix = ".Otp";
      break;
    case VirtualCardUnmaskFlowType::kOtpFallbackFromFido:
      flow_type_suffix = ".OtpFallbackFromFido";
      break;
  }

  base::UmaHistogramEnumeration(
      "Autofill.ServerCardUnmask" +
          AutofillMetrics::GetHistogramStringForCardType(card_type) +
          ".Result" + flow_type_suffix,
      unmask_result);
}

void LogServerCardUnmaskFormSubmission(
    AutofillClient::PaymentsRpcCardType card_type) {
  base::UmaHistogramBoolean(
      "Autofill.ServerCardUnmask" +
          AutofillMetrics::GetHistogramStringForCardType(card_type) +
          ".FormSubmission",
      true);
}

}  // namespace autofill::autofill_metrics

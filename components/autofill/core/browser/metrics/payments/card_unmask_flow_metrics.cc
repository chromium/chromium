// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

void LogServerCardUnmaskAttempt(
    payments::PaymentsAutofillClient::PaymentsRpcCardType card_type) {
  base::UmaHistogramBoolean(
      "Autofill.ServerCardUnmask" +
          AutofillMetrics::GetHistogramStringForCardType(card_type) +
          ".Attempt",
      true);
}

void LogCvcFilling(CvcFillingFlowType flow_type,
                   CreditCard::RecordType record_type) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Autofill.CvcStorage.CvcFilling",
           AutofillMetrics::GetHistogramStringForCardType(record_type)}),
      flow_type);
}

void LogServerCardUnmaskResult(
    ServerCardUnmaskResult unmask_result,
    absl::variant<payments::PaymentsAutofillClient::PaymentsRpcCardType,
                  CreditCard::RecordType> card_type,
    ServerCardUnmaskFlowType flow_type) {
  std::string flow_type_suffix;
  switch (flow_type) {
    case ServerCardUnmaskFlowType::kUnspecified:
      flow_type_suffix = ".UnspecifiedFlowType";
      break;
    case ServerCardUnmaskFlowType::kFidoOnly:
      flow_type_suffix = ".Fido";
      break;
    case ServerCardUnmaskFlowType::kOtpOnly:
      flow_type_suffix = ".Otp";
      break;
    case ServerCardUnmaskFlowType::kOtpFallbackFromFido:
      flow_type_suffix = ".OtpFallbackFromFido";
      break;
    case ServerCardUnmaskFlowType::kRiskBased:
      flow_type_suffix = ".RiskBased";
      break;
    case ServerCardUnmaskFlowType::kDeviceUnlock:
      flow_type_suffix = ".DeviceUnlock";
      break;
    case ServerCardUnmaskFlowType::kThreeDomainSecure:
      flow_type_suffix = ".ThreeDomainSecure";
      break;
  }

  base::UmaHistogramEnumeration(
      "Autofill.ServerCardUnmask" +
          AutofillMetrics::GetHistogramStringForCardType(card_type) +
          ".Result" + flow_type_suffix,
      unmask_result);
}

void LogServerCardUnmaskFormSubmission(
    payments::PaymentsAutofillClient::PaymentsRpcCardType card_type) {
  base::UmaHistogramBoolean(
      "Autofill.ServerCardUnmask" +
          AutofillMetrics::GetHistogramStringForCardType(card_type) +
          ".FormSubmission",
      true);
}

}  // namespace autofill::autofill_metrics

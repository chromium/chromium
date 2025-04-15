// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"

namespace autofill::autofill_metrics {

void LogCvcAuthAttempt(CreditCard::RecordType card_type) {
  std::string card_type_histogram_string =
      AutofillMetrics::GetHistogramStringForCardType(card_type);
  base::UmaHistogramBoolean(
      "Autofill.CvcAuth" + card_type_histogram_string + ".Attempt", true);
}

void LogCvcAuthResult(CreditCard::RecordType card_type, CvcAuthEvent event) {
  std::string card_type_histogram_string =
      AutofillMetrics::GetHistogramStringForCardType(card_type);
  base::UmaHistogramEnumeration(
      "Autofill.CvcAuth" + card_type_histogram_string + ".Result", event);
}

void LogCvcAuthRetryableError(CreditCard::RecordType card_type,
                              CvcAuthEvent event) {
  std::string card_type_histogram_string =
      AutofillMetrics::GetHistogramStringForCardType(card_type);
  base::UmaHistogramEnumeration(
      "Autofill.CvcAuth" + card_type_histogram_string + ".RetryableError",
      event);
}

void LogOtpAuthAttempt(CardUnmaskChallengeOptionType type) {
  base::UmaHistogramBoolean(
      "Autofill.OtpAuth." + GetOtpAuthType(type) + ".Attempt", true);
}

void LogOtpAuthResult(OtpAuthEvent event, CardUnmaskChallengeOptionType type) {
  DCHECK_LE(event, OtpAuthEvent::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.OtpAuth." + GetOtpAuthType(type) + ".Result", event);
}

void LogOtpAuthRetriableError(OtpAuthEvent event,
                              CardUnmaskChallengeOptionType type) {
  DCHECK_LE(event, OtpAuthEvent::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.OtpAuth." + GetOtpAuthType(type) + ".RetriableError", event);
}

void LogOtpAuthUnmaskCardRequestLatency(base::TimeDelta duration,
                                        CardUnmaskChallengeOptionType type) {
  base::UmaHistogramLongTimes("Autofill.OtpAuth." + GetOtpAuthType(type) +
                                  ".RequestLatency.UnmaskCardRequest",
                              duration);
}

void LogOtpAuthSelectChallengeOptionRequestLatency(
    base::TimeDelta duration,
    CardUnmaskChallengeOptionType type) {
  base::UmaHistogramLongTimes(
      "Autofill.OtpAuth." + GetOtpAuthType(type) +
          ".RequestLatency.SelectChallengeOptionRequest",
      duration);
}

void LogOtpInputDialogShown(CardUnmaskChallengeOptionType type) {
  base::UmaHistogramBoolean(
      "Autofill.OtpInputDialog." + GetOtpAuthType(type) + ".Shown", true);
}

void LogOtpInputDialogResult(OtpInputDialogResult result,
                             bool temporary_error_shown,
                             CardUnmaskChallengeOptionType type) {
  DCHECK_GT(result, OtpInputDialogResult::kUnknown);
  DCHECK_LE(result, OtpInputDialogResult::kMaxValue);
  std::string temporary_error_shown_suffix = temporary_error_shown
                                                 ? ".WithPreviousTemporaryError"
                                                 : ".WithNoTemporaryError";
  base::UmaHistogramEnumeration(
      "Autofill.OtpInputDialog." + GetOtpAuthType(type) + ".Result", result);
  base::UmaHistogramEnumeration("Autofill.OtpInputDialog." +
                                    GetOtpAuthType(type) + ".Result" +
                                    temporary_error_shown_suffix,
                                result);
}

void LogOtpInputDialogErrorMessageShown(OtpInputDialogError error,
                                        CardUnmaskChallengeOptionType type) {
  DCHECK_GT(error, OtpInputDialogError::kUnknown);
  DCHECK_LE(error, OtpInputDialogError::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.OtpInputDialog." + GetOtpAuthType(type) + ".ErrorMessageShown",
      error);
}

void LogOtpInputDialogNewOtpRequested(CardUnmaskChallengeOptionType type) {
  base::UmaHistogramBoolean(
      "Autofill.OtpInputDialog." + GetOtpAuthType(type) + ".NewOtpRequested",
      true);
}

std::string GetOtpAuthType(CardUnmaskChallengeOptionType type) {
  if (type == CardUnmaskChallengeOptionType::kSmsOtp) {
    return "SmsOtp";
  } else if (type == CardUnmaskChallengeOptionType::kEmailOtp) {
    return "EmailOtp";
  }
  NOTREACHED();
}

void LogRiskBasedAuthAttempt(CreditCard::RecordType card_type) {
  std::string card_type_histogram_string =
      AutofillMetrics::GetHistogramStringForCardType(card_type);
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Autofill.RiskBasedAuth", card_type_histogram_string, ".Attempt"}),
      true);
}

void LogRiskBasedAuthResult(CreditCard::RecordType card_type,
                            RiskBasedAuthEvent event) {
  std::string card_type_histogram_string =
      AutofillMetrics::GetHistogramStringForCardType(card_type);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Autofill.RiskBasedAuth", card_type_histogram_string, ".Result"}),
      event);
}

void LogRiskBasedAuthLatency(base::TimeDelta duration,
                             CreditCard::RecordType card_type) {
  base::UmaHistogramLongTimes(
      "Autofill.RiskBasedAuth" +
          AutofillMetrics::GetHistogramStringForCardType(card_type) +
          ".Latency",
      duration);
}

}  // namespace autofill::autofill_metrics

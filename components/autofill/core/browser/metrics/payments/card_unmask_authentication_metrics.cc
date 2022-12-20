// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

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

void LogOtpAuthAttempt() {
  base::UmaHistogramBoolean("Autofill.OtpAuth.SmsOtp.Attempt", true);
}

void LogOtpAuthResult(OtpAuthEvent event) {
  DCHECK_LE(event, OtpAuthEvent::kMaxValue);
  base::UmaHistogramEnumeration("Autofill.OtpAuth.SmsOtp.Result", event);
}

void LogOtpAuthRetriableError(OtpAuthEvent event) {
  DCHECK_LE(event, OtpAuthEvent::kMaxValue);
  base::UmaHistogramEnumeration("Autofill.OtpAuth.SmsOtp.RetriableError",
                                event);
}

void LogOtpAuthUnmaskCardRequestLatency(const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes(
      "Autofill.OtpAuth.SmsOtp.RequestLatency.UnmaskCardRequest", duration);
}

void LogOtpAuthSelectChallengeOptionRequestLatency(
    const base::TimeDelta& duration) {
  base::UmaHistogramLongTimes(
      "Autofill.OtpAuth.SmsOtp.RequestLatency.SelectChallengeOptionRequest",
      duration);
}

void LogOtpInputDialogShown() {
  base::UmaHistogramBoolean("Autofill.OtpInputDialog.SmsOtp.Shown", true);
}

void LogOtpInputDialogResult(OtpInputDialogResult result,
                             bool temporary_error_shown) {
  DCHECK_GT(result, OtpInputDialogResult::kUnknown);
  DCHECK_LE(result, OtpInputDialogResult::kMaxValue);
  std::string temporary_error_shown_suffix = temporary_error_shown
                                                 ? ".WithPreviousTemporaryError"
                                                 : ".WithNoTemporaryError";
  base::UmaHistogramEnumeration("Autofill.OtpInputDialog.SmsOtp.Result",
                                result);
  base::UmaHistogramEnumeration(
      "Autofill.OtpInputDialog.SmsOtp.Result" + temporary_error_shown_suffix,
      result);
}

void LogOtpInputDialogErrorMessageShown(OtpInputDialogError error) {
  DCHECK_GT(error, OtpInputDialogError::kUnknown);
  DCHECK_LE(error, OtpInputDialogError::kMaxValue);
  base::UmaHistogramEnumeration(
      "Autofill.OtpInputDialog.SmsOtp.ErrorMessageShown", error);
}

void LogOtpInputDialogNewOtpRequested() {
  base::UmaHistogramBoolean("Autofill.OtpInputDialog.SmsOtp.NewOtpRequested",
                            true);
}

}  // namespace autofill::autofill_metrics

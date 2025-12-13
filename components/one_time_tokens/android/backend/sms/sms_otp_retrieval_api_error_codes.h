// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERROR_CODES_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERROR_CODES_H_

namespace one_time_tokens {

// Error codes returned by the Android SMS Retriever API.
// This list might be extended in the future if there are other commonly
// returned error codes worth labeling nicely in metrics enums.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange
enum class SmsOtpRetrievalApiErrorCode {
  // Mapped from OneTimeTokensBackendErrorCode.GMSCORE_VERSION_NOT_SUPPORTED
  kGmscoreVersionNotSupported = 0,
  // CommonStatusCodes.ERROR
  kError = 13,
  // timeout
  kTimeout = 15,
  // SmsCodeAutofillClient not supported for platforms before Android P
  kPlatformNotSupported = 36500,
  // calling application is not eligible to use SmsCodeAutofillClient
  kApiNotAvailable = 36501,
  // permission denied by the user.
  kUserPermissionRequired = 36502
};
// LINT.ThenChange(
//   //components/one_time_tokens/android/backend/sms/sms_otp_to_one_time_token_retrieval_error_converter.cc,
//   //tools/metrics/histograms/metadata/autofill/enums.xml
// )

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERROR_CODES_H_

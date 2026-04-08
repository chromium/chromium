// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERRORS_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERRORS_H_

namespace one_time_tokens {

// Error codes returned by the Android SMS Retriever API.
// This list might be extended in the future if there are other commonly
// returned error codes worth labeling nicely in metrics enums.
//
// LINT.IfChange
enum class SmsOtpRetrievalApiError {
  // Unrecognized GMS Core error code.
  kUnknown,
  // Mapped from OneTimeTokensBackendErrorCode.GMSCORE_VERSION_NOT_SUPPORTED
  kGmscoreVersionNotSupported,
  // CommonStatusCodes.ERROR
  kError,
  // timeout
  kTimeout,
  // SmsCodeAutofillClient not supported for platforms before Android P
  kPlatformNotSupported,
  // calling application is not eligible to use SmsCodeAutofillClient
  kApiNotAvailable,
  // permission denied by the user.
  kUserPermissionRequired,
  kMaxValue = kUserPermissionRequired,
};
// LINT.ThenChange(
//   //components/one_time_tokens/android/backend/sms/sms_otp_to_one_time_token_retrieval_error_converter.cc,
//   //tools/metrics/histograms/metadata/autofill/enums.xml
// )

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERRORS_H_

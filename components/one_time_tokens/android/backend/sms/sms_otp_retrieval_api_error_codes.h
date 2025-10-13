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
//
// Needs to be kept in sync with SmsOtpRetrievalApiErrorCode in
// tools/metrics/histograms/enums.xml.
enum class SmsOtpRetrievalApiErrorCode {
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

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERROR_CODES_H_

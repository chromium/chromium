// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERROR_CODES_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERROR_CODES_H_

namespace one_time_tokens {

// Error codes returned by the Android SMS Retriever API.
// This list might be extended in the future if there are other commonly
// returned error codes worth labeling nicely in metrics enums.
enum class SmsOtpRetrievalApiErrorCode {
  kTimeout = 15,
  kPlatformNotSupported = 36500,
  kApiNotAvailable = 36501,
  kUserPermissionRequired = 36502
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_RETRIEVAL_API_ERROR_CODES_H_

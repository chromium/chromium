// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_TO_ONE_TIME_TOKEN_RETRIEVAL_ERROR_CONVERTER_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_TO_ONE_TIME_TOKEN_RETRIEVAL_ERROR_CONVERTER_H_

namespace one_time_tokens {

enum class OneTimeTokenRetrievalError;
enum class SmsOtpRetrievalApiErrorCode;

OneTimeTokenRetrievalError ConvertSmsOtpRetrievalApiErrorCode(
    SmsOtpRetrievalApiErrorCode error_code);

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_SMS_OTP_TO_ONE_TIME_TOKEN_RETRIEVAL_ERROR_CONVERTER_H_

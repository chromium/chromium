// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/android/backend/sms/sms_otp_to_one_time_token_retrieval_error_converter.h"

#include "components/one_time_tokens/android/backend/sms/sms_otp_retrieval_api_error_codes.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

TEST(SmsOtpToOnetimeTokenRetrievalErrorConverterTest, Smoke) {
  EXPECT_EQ(
      ConvertSmsOtpRetrievalApiErrorCode(SmsOtpRetrievalApiErrorCode::kError),
      OneTimeTokenRetrievalError::kSmsOtpBackendError);
  EXPECT_EQ(
      ConvertSmsOtpRetrievalApiErrorCode(SmsOtpRetrievalApiErrorCode::kTimeout),
      OneTimeTokenRetrievalError::kSmsOtpBackendTimeout);
  EXPECT_EQ(ConvertSmsOtpRetrievalApiErrorCode(
                SmsOtpRetrievalApiErrorCode::kPlatformNotSupported),
            OneTimeTokenRetrievalError::kSmsOtpBackendPlatformNotSupported);
  EXPECT_EQ(ConvertSmsOtpRetrievalApiErrorCode(
                SmsOtpRetrievalApiErrorCode::kApiNotAvailable),
            OneTimeTokenRetrievalError::kSmsOtpBackendApiNotAvailable);
  EXPECT_EQ(ConvertSmsOtpRetrievalApiErrorCode(
                SmsOtpRetrievalApiErrorCode::kUserPermissionRequired),
            OneTimeTokenRetrievalError::kSmsOtpBackendUserPermissionRequired);
  EXPECT_EQ(ConvertSmsOtpRetrievalApiErrorCode(
                SmsOtpRetrievalApiErrorCode::kGmscoreVersionNotSupported),
            OneTimeTokenRetrievalError::kSmsOtpGmscoreVersionNotSupported);
}

}  // namespace one_time_tokens

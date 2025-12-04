// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/android/backend/sms/sms_otp_to_one_time_token_retrieval_error_converter.h"

#include "components/one_time_tokens/android/backend/sms/sms_otp_retrieval_api_error_codes.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace one_time_tokens {

OneTimeTokenRetrievalError ConvertSmsOtpRetrievalApiErrorCode(
    SmsOtpRetrievalApiErrorCode error_code) {
  // LINT.IfChange
  switch (error_code) {
    case SmsOtpRetrievalApiErrorCode::kGmscoreVersionNotSupported:
      return OneTimeTokenRetrievalError::kSmsOtpGmscoreVersionNotSupported;
    case SmsOtpRetrievalApiErrorCode::kError:
      return OneTimeTokenRetrievalError::kSmsOtpBackendError;
    case SmsOtpRetrievalApiErrorCode::kTimeout:
      return OneTimeTokenRetrievalError::kSmsOtpBackendTimeout;
    case SmsOtpRetrievalApiErrorCode::kPlatformNotSupported:
      return OneTimeTokenRetrievalError::kSmsOtpBackendPlatformNotSupported;
    case SmsOtpRetrievalApiErrorCode::kApiNotAvailable:
      return OneTimeTokenRetrievalError::kSmsOtpBackendApiNotAvailable;
    case SmsOtpRetrievalApiErrorCode::kUserPermissionRequired:
      return OneTimeTokenRetrievalError::kSmsOtpBackendUserPermissionRequired;
  }
  // LINT.ThenChange(//components/one_time_tokens/core/browser/one_time_token_retrieval_error.h)
}

}  // namespace one_time_tokens

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_SMS_OTP_BACKEND_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_SMS_OTP_BACKEND_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace one_time_tokens {

// Abstract interface for fetching OTPs sent via SMS.
class SmsOtpBackend {
 public:
  virtual ~SmsOtpBackend();

  // Queries the backend for recently received OTPs.
  virtual void RetrieveSmsOtp(
      base::OnceCallback<
          void(base::expected<OneTimeToken, OneTimeTokenRetrievalError>)>
          callback) = 0;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_SMS_OTP_BACKEND_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_SMS_OTP_BACKEND_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_SMS_OTP_BACKEND_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"

namespace one_time_tokens {

struct OtpFetchReply {
  OtpFetchReply(std::optional<OneTimeToken> otp_value, bool request_complete);
  OtpFetchReply(const OtpFetchReply& rhs);
  OtpFetchReply& operator=(const OtpFetchReply& rhs);
  ~OtpFetchReply();

  std::optional<OneTimeToken> otp_value;
  // True if the request completed successfully. True if OTP value could be
  // fetched, or if the OTP value was not found within teh allowed timeframe.
  // False if the request is not complete (e.g. due to fetching backend API
  // not available, or user permission denied).
  bool request_complete = false;
};

// Abstract interface for fetching OTPs sent via SMS.
class SmsOtpBackend {
 public:
  virtual ~SmsOtpBackend();

  // Queries the backend for recently received OTPs.
  virtual void RetrieveSmsOtp(
      base::OnceCallback<void(const OtpFetchReply&)> callback) = 0;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_SMS_OTP_BACKEND_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_SMS_OTP_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_SMS_OTP_BACKEND_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"

namespace password_manager {

struct OtpFetchReply {
  OtpFetchReply(std::optional<std::string> otp_value, bool request_complete);
  OtpFetchReply(const OtpFetchReply& rhs);
  OtpFetchReply& operator=(const OtpFetchReply& rhs);
  ~OtpFetchReply();

  std::optional<std::string> otp_value;
  // True if the request completed successfully. True if OTP value could be
  // fetched, or if the OTP value was not found within teh allowed timeframe.
  // False if the request is not complete (e.g. due to fetching backend API
  // not available, or user permission denied).
  bool request_complete = false;
};

// Abstract interface for fetching OTPs sent via SMS.
class SmsOtpBackend {
 public:
  // Queries the backend for recently received OTPs.
  virtual void RetrieveSmsOtp(
      base::OnceCallback<void(const password_manager::OtpFetchReply&)>
          callback) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_SMS_OTP_BACKEND_H_

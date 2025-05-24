// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_SMS_OTP_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_SMS_OTP_BACKEND_H_

namespace password_manager {

// Abstract interface for fetching OTPs sent via SMS.
class SmsOtpBackend {
 public:
  // Queries the backend for recently received OTPs.
  virtual void RetrieveSmsOtp() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_SMS_OTP_BACKEND_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/authentication_error.h"

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"

namespace ash {

AuthenticationError::AuthenticationError(cryptohome::ErrorWrapper wrapper)
    : origin_(Origin::kCryptohome), cryptohome_error_(wrapper) {}

AuthenticationError::AuthenticationError(
    AuthFailure::FailureReason auth_failure_reason)
    : AuthenticationError(AuthFailure(auth_failure_reason)) {}

AuthenticationError::AuthenticationError(AuthFailure auth_failure)
    : origin_(Origin::kChrome),
      cryptohome_error_(cryptohome::ErrorWrapper::success()),
      auth_failure_(std::move(auth_failure)) {}

AuthenticationError::~AuthenticationError() = default;

void AuthenticationError::ResolveToFailure(
    AuthFailure::FailureReason auth_failure_reason) {
  auth_failure_ = AuthFailure{auth_failure_reason};
}

std::string AuthenticationError::ToDebugString() const {
  std::string result = "AuthenticationError: ";
  if (origin_ == Origin::kCryptohome) {
    base::StrAppend(&result, {base::StringPrintf(
                                 "Origin: Cryptohome, CryptohomeErrorCode: %d",
                                 static_cast<int>(get_cryptohome_code()))});
  } else if (origin_ == Origin::kChrome) {
    base::StrAppend(&result, {base::StringPrintf(
                                 "Origin: Chrome, FailureReason: %d",
                                 static_cast<int>(get_resolved_failure()))});
    if (get_resolved_failure() ==
        AuthFailure::FailureReason::CRYPTOHOME_RECOVERY_SERVICE_ERROR) {
      base::StrAppend(
          &result,
          {base::StringPrintf(
              ", CryptohomeRecoveryServerStatusCode: %d",
              static_cast<int>(get_cryptohome_recovery_server_error()))});
    }
  }
  return result;
}

}  // namespace ash

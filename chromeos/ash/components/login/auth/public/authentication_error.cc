// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/authentication_error.h"

namespace ash {

AuthenticationError::AuthenticationError(
    user_data_auth::CryptohomeErrorCode cryptohome_code)
    : origin_(Origin::kCryptohome), cryptohome_code_(cryptohome_code) {}

AuthenticationError::AuthenticationError(
    AuthFailure::FailureReason auth_failure_reason)
    : AuthenticationError(AuthFailure(auth_failure_reason)) {}

AuthenticationError::AuthenticationError(AuthFailure auth_failure)
    : origin_(Origin::kChrome), auth_failure_(std::move(auth_failure)) {}

AuthenticationError::~AuthenticationError() = default;

void AuthenticationError::ResolveToFailure(
    AuthFailure::FailureReason auth_failure_reason) {
  auth_failure_ = AuthFailure{auth_failure_reason};
}

}  // namespace ash

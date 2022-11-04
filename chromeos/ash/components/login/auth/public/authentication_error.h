// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTHENTICATION_ERROR_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTHENTICATION_ERROR_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"

namespace ash {

// Struct that wraps various errors that can happen during login/authentication.

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC)
    AuthenticationError {
 public:
  enum class Origin {
    // The error comes from `cryptohomed`.
    kCryptohome,
    // The error represents some erroneous state detected by the chrome.
    kChrome,
  };
  explicit AuthenticationError(
      user_data_auth::CryptohomeErrorCode cryptohome_code);
  explicit AuthenticationError(AuthFailure::FailureReason auth_failure_reason);
  explicit AuthenticationError(AuthFailure auth_failure);

  ~AuthenticationError();

  Origin get_origin() const { return origin_; }

  AuthFailure::FailureReason get_resolved_failure() const {
    return auth_failure_.reason();
  }

  void ResolveToFailure(AuthFailure::FailureReason auth_failure_reason);

  user_data_auth::CryptohomeErrorCode get_cryptohome_code() const {
    return cryptohome_code_;
  }

 private:
  Origin origin_;
  // Cryptohome-specific fields:
  user_data_auth::CryptohomeErrorCode cryptohome_code_;

  // Mapping of the `error_code` to auth flow failure reason.
  AuthFailure auth_failure_{AuthFailure::NONE};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTHENTICATION_ERROR_H_

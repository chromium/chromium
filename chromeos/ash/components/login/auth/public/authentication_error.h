// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTHENTICATION_ERROR_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTHENTICATION_ERROR_H_

#include "base/component_export.h"
#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/recovery_types.h"

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
  // explicit AuthenticationError(::user_data_auth::CryptohomeErrorCode
  // cryptohome_code);
  explicit AuthenticationError(cryptohome::ErrorWrapper wrapper);
  explicit AuthenticationError(AuthFailure::FailureReason auth_failure_reason);
  explicit AuthenticationError(AuthFailure auth_failure);

  ~AuthenticationError();

  Origin get_origin() const { return origin_; }

  AuthFailure::FailureReason get_resolved_failure() const {
    return auth_failure_.reason();
  }

  CryptohomeRecoveryServerStatusCode get_cryptohome_recovery_server_error()
      const {
    return auth_failure_.cryptohome_recovery_server_error();
  }

  void ResolveToFailure(AuthFailure::FailureReason auth_failure_reason);

  // CryptohomeErrorCode is the legacy error code and will be removed in the
  // future. This function is kept here for compatibility during migration.
  ::user_data_auth::CryptohomeErrorCode get_cryptohome_code() const {
    return cryptohome_error_.code();
  }

  // ErrorWrapper holds the new CryptohomeErrorInfo structure for representing
  // error. New code should use this instead.
  cryptohome::ErrorWrapper get_cryptohome_error() const {
    return cryptohome_error_;
  }

  std::string ToDebugString() const;

 private:
  Origin origin_;
  // Cryptohome-specific fields:
  cryptohome::ErrorWrapper cryptohome_error_;

  // Mapping of the `error_code` to auth flow failure reason.
  AuthFailure auth_failure_{AuthFailure::NONE};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTHENTICATION_ERROR_H_

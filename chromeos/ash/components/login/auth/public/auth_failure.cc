// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/auth_failure.h"

#include "base/check_op.h"

namespace ash {

AuthFailure::AuthFailure(FailureReason reason) : reason_(reason) {
  DCHECK_NE(reason, NETWORK_AUTH_FAILED);
  DCHECK_NE(reason, CRYPTOHOME_RECOVERY_SERVICE_ERROR);
}

// private
AuthFailure::AuthFailure(GoogleServiceAuthError error)
    : reason_(NETWORK_AUTH_FAILED), google_service_auth_error_(error) {}

AuthFailure::AuthFailure(CryptohomeRecoveryServerStatusCode status_code)
    : reason_(CRYPTOHOME_RECOVERY_SERVICE_ERROR),
      cryptohome_recovery_server_error_(status_code) {
  DCHECK_NE(status_code, CryptohomeRecoveryServerStatusCode::kSuccess);
}

// static
AuthFailure AuthFailure::FromNetworkAuthFailure(GoogleServiceAuthError error) {
  return AuthFailure(std::move(error));
}

const std::string AuthFailure::GetErrorString() const {
  switch (reason_) {
    case DATA_REMOVAL_FAILED:
      return "Could not destroy your old data.";
    case COULD_NOT_MOUNT_CRYPTOHOME:
      return "Could not mount cryptohome.";
    case COULD_NOT_UNMOUNT_CRYPTOHOME:
      return "Could not unmount cryptohome.";
    case COULD_NOT_MOUNT_TMPFS:
      return "Could not mount tmpfs.";
    case LOGIN_TIMED_OUT:
      return "Login timed out. Please try again.";
    case UNLOCK_FAILED:
      return "Unlock failed.";
    case NETWORK_AUTH_FAILED:
      if (google_service_auth_error_.state() ==
          GoogleServiceAuthError::CONNECTION_FAILED) {
        return net::ErrorToString(google_service_auth_error_.network_error());
      }
      return "Google authentication failed.";
    case OWNER_REQUIRED:
      return "Login is restricted to the owner's account only.";
    case ALLOWLIST_CHECK_FAILED:
      return "Login attempt blocked by allowlist.";
    case FAILED_TO_INITIALIZE_TOKEN:
      return "OAuth2 token fetch failed.";
    case MISSING_CRYPTOHOME:
      return "Cryptohome missing from disk.";
    case AUTH_DISABLED:
      return "Auth disabled for user.";
    case TPM_ERROR:
      return "Critical TPM error encountered.";
    case TPM_UPDATE_REQUIRED:
      return "TPM firmware update required.";
    case UNRECOVERABLE_CRYPTOHOME:
      return "Cryptohome is corrupted.";
    case USERNAME_HASH_FAILED:
      return "Failed to get hashed username";
    case CRYPTOHOME_RECOVERY_SERVICE_ERROR:
      return "Failed interaction with cryptohome recovery server";
    case CRYPTOHOME_RECOVERY_OAUTH_TOKEN_ERROR:
      return "Failed to fetch OAuth2 token for recovery service";
    case NONE:
    case NUM_FAILURE_REASONS:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

}  // namespace ash

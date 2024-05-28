// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_RECOVERY_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_RECOVERY_TYPES_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/types/strong_alias.h"

namespace ash {

// Gaia access token that is used to call Recovery service APIs.
using GaiaAccessToken =
    base::StrongAlias<class GaiaAccessTokenTag, std::string>;

// Gaia reauth proof token that is used as a proof that the user has passed
// online signin.
using GaiaReauthProofToken =
    base::StrongAlias<class GaiaReauthProofTokenTag, std::string>;

// Recovery request (`nonce`) issued by cryptohome.
using RecoveryRequest =
    base::StrongAlias<class RecoveryRequestTag, std::string>;

// Serialized proto response for FetchEpoch request.
using CryptohomeRecoveryEpochResponse =
    base::StrongAlias<class CryptohomeRecoveryEpochResponseTag,
                      std::vector<uint8_t>>;

// Serialized proto response for FetchRecoveryResponse request.
using CryptohomeRecoveryResponse =
    base::StrongAlias<class CryptohomeRecoveryResponseTag,
                      std::vector<uint8_t>>;

using RecoveryLedgerName =
    base::StrongAlias<class RecoveryLedgerNameTag, std::string>;
using RecoveryLedgerPubKey =
    base::StrongAlias<class RecoveryLedgerPubKeyTag, std::string>;

enum class CryptohomeRecoveryServerStatusCode {
  // The request was completed successfully.
  kSuccess,
  // Failed with network error.
  kNetworkError,
  // Failed with HTTP 401 (Unauthorized) error.
  kAuthError,
  // Failed with HTTP 500-599 error (Server error).
  kServerError,
  // Failed with other errors, that are not retriable.
  kFatalError,
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_RECOVERY_TYPES_H_

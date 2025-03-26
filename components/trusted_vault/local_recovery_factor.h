// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_LOCAL_RECOVERY_FACTOR_H_
#define COMPONENTS_TRUSTED_VAULT_LOCAL_RECOVERY_FACTOR_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

// Interface for a local recovery factor.
// Classes that implement this interface are used by
// StandaloneTrustedVaultBackend to retrieve keys without user interaction when
// required.
// StandaloneTrustedVaultBackend also makes sure to register local recovery
// factors with available keys when possible.
// All operations on LocalRecoveryFactor need to be performed on the same
// sequence as StandaloneTrustedVaultBackend.
class LocalRecoveryFactor {
 public:
  using AttemptRecoveryCallback = base::OnceCallback<void(
      TrustedVaultDownloadKeysStatus /* status */,
      const std::vector<std::vector<uint8_t>>& /* new_vault_keys */,
      int /* last_vault_key_version */)>;
  using AttemptRecoveryFailureCallback = base::OnceCallback<void(
      std::optional<TrustedVaultDownloadKeysStatusForUMA> /* status */)>;
  using RegisterCallback =
      base::OnceCallback<void(TrustedVaultRegistrationStatus /* status */,
                              int /* key_version */,
                              bool /* had_local_keys */)>;

  LocalRecoveryFactor() = default;
  LocalRecoveryFactor(const LocalRecoveryFactor&) = delete;
  LocalRecoveryFactor& operator=(const LocalRecoveryFactor&) = delete;
  virtual ~LocalRecoveryFactor() = default;

  // Attempts a key recovery.
  // Note: If `connection_requests_throttled` is true, implementations of this
  // method are not allowed to make requests to `connection`.
  virtual void AttemptRecovery(TrustedVaultConnection* connection,
                               bool connection_requests_throttled,
                               AttemptRecoveryCallback cb,
                               AttemptRecoveryFailureCallback failure_cb) = 0;

  // Marks the recovery factor as not registered, which makes it eligible for
  // future registration attempts.
  virtual void MarkAsNotRegistered() = 0;
  // Clears information about any potential previous registration attempts.
  // This can be called for accounts other than the account this recovery
  // factor was created for, thus `gaia_id` is passed in explicitly.
  virtual void ClearRegistrationAttemptInfo(const GaiaId& gaia_id) = 0;
  // Attempts to register the recovery factor in case it's not yet registered
  // and currently available local data is sufficient to do it. It returns an
  // enum representing the registration state, intended to be used for metric
  // recording.
  // Note: If `connection_requests_throttled` is true, implementations of this
  // method are not allowed to make requests to `connection`.
  virtual TrustedVaultDeviceRegistrationStateForUMA MaybeRegister(
      TrustedVaultConnection* connection,
      bool connection_requests_throttled,
      RegisterCallback cb) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_LOCAL_RECOVERY_FACTOR_H_

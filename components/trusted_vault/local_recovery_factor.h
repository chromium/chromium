// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_LOCAL_RECOVERY_FACTOR_H_
#define COMPONENTS_TRUSTED_VAULT_LOCAL_RECOVERY_FACTOR_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

// Type of a LocalRecoveryFactor. Overwritten by sub-classes according to how
// they manage recovery keys locally.
enum class LocalRecoveryFactorType {
  kPhysicalDevice,
#if BUILDFLAG(IS_MAC)
  kICloudKeychain,
#endif
};

// Interface for a local recovery factor.
// Classes that implement this interface are used by
// StandaloneTrustedVaultBackend to recover keys without user interaction when
// required.
// StandaloneTrustedVaultBackend also makes sure to register local recovery
// factors with available keys when possible.
// All operations on LocalRecoveryFactor need to be performed on the same
// sequence as StandaloneTrustedVaultBackend.
class LocalRecoveryFactor {
 public:
  enum class RecoveryStatus {
    // Keys were successfully recovered.
    kSuccess,
    // Failed to recover keys.
    kFailure,
    // Keys were successfully recovered and verified, but no new keys exist.
    kNoNewKeys,
  };

  using AttemptRecoveryCallback = base::OnceCallback<void(
      RecoveryStatus /* status */,
      const std::vector<std::vector<uint8_t>>& /* new_vault_keys */,
      int /* last_vault_key_version */)>;
  using RegisterCallback =
      base::OnceCallback<void(TrustedVaultRegistrationStatus /* status */,
                              int /* key_version */,
                              bool /* had_local_keys */)>;

  LocalRecoveryFactor() = default;
  LocalRecoveryFactor(const LocalRecoveryFactor&) = delete;
  LocalRecoveryFactor& operator=(const LocalRecoveryFactor&) = delete;
  virtual ~LocalRecoveryFactor() = default;

  // Returns the type of this local recovery factor.
  virtual LocalRecoveryFactorType GetRecoveryFactorType() const = 0;

  // Attempts a key recovery.
  virtual void AttemptRecovery(AttemptRecoveryCallback cb) = 0;

  // Returns whether the recovery factor is marked as registered.
  virtual bool IsRegistered() = 0;
  // Marks the recovery factor as not registered, which makes it eligible for
  // future registration attempts.
  virtual void MarkAsNotRegistered() = 0;
  // Attempts to register the recovery factor in case it's not yet registered
  // and currently available local data is sufficient to do it. It returns an
  // enum representing the registration state, intended to be used for metric
  // recording.
  virtual TrustedVaultRecoveryFactorRegistrationStateForUMA MaybeRegister(
      RegisterCallback cb) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_LOCAL_RECOVERY_FACTOR_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

enum class LocalRecoveryFactorType;
enum class SecurityDomainId;

using LocalDeviceRegistrationInfo =
    trusted_vault_pb::LocalDeviceRegistrationInfo;

// Storage interface for local recovery factor registration state.
class RecoveryFactorRegistrationStorage {
 public:
  virtual ~RecoveryFactorRegistrationStorage() = default;

  // Returns whether `factor_type` is registered for `gaia_id` in `domain`.
  // Returns `false` if the user vault or domain data does not exist.
  virtual bool IsRecoveryFactorRegistered(
      const GaiaId& gaia_id,
      SecurityDomainId domain,
      LocalRecoveryFactorType factor_type) const = 0;

  // Sets whether `factor_type` is registered for `gaia_id` in `domain`.
  // Creates the user vault if it does not exist, and commits changes to disk.
  virtual void SetRecoveryFactorRegistered(const GaiaId& gaia_id,
                                           SecurityDomainId domain,
                                           LocalRecoveryFactorType factor_type,
                                           bool registered) = 0;

  // Returns whether last registration returned local data obsolete for
  // `gaia_id` in `domain`.
  // Returns `false` if the user vault or domain data does not exist.
  virtual bool GetLastRegistrationReturnedLocalDataObsolete(
      const GaiaId& gaia_id,
      SecurityDomainId domain) const = 0;

  // Sets whether last registration returned local data obsolete for `gaia_id`
  // in `domain`.
  // Creates the user vault if it does not exist, and commits changes to disk.
  virtual void SetLastRegistrationReturnedLocalDataObsolete(
      const GaiaId& gaia_id,
      SecurityDomainId domain,
      bool obsolete) = 0;
};

// Storage interface for physical device key material.
// Note: Physical device private keys are domain-independent and shared across
// domains.
class PhysicalDeviceStorage {
 public:
  virtual ~PhysicalDeviceStorage() = default;

  // Finds and returns a reference to the local device registration info for
  // `gaia_id`.
  // Returns a default instance if the user vault does not exist.
  virtual const LocalDeviceRegistrationInfo& GetLocalDeviceRegistrationInfo(
      const GaiaId& gaia_id) const = 0;

  // Mutates the local device registration info for `gaia_id` and commits the
  // changes to disk.
  // Creates the user vault if it does not exist.
  virtual void MutateLocalDeviceRegistrationInfo(
      const GaiaId& gaia_id,
      base::FunctionRef<void(LocalDeviceRegistrationInfo&)> mutator) = 0;
};

// Storage interface for managing vault keys across security domains.
class KeyStorage {
 public:
  virtual ~KeyStorage() = default;

  // Finds and returns all vault keys for `gaia_id` in `domain`.
  // Returns an empty vector if the user vault or domain data does not exist.
  virtual std::vector<std::vector<uint8_t>> GetVaultKeys(
      const GaiaId& gaia_id,
      SecurityDomainId domain) const = 0;

  // Returns the version corresponding to the last vault key for `gaia_id` in
  // `domain`.
  // Returns 0 if the user vault or domain data does not exist.
  virtual int GetLastKeyVersion(const GaiaId& gaia_id,
                                SecurityDomainId domain) const = 0;

  // Replaces all vault keys and updates last vault key version for `gaia_id` in
  // `domain`.
  // Resets keys_marked_as_stale_by_consumer to false, creates the user vault if
  // it does not exist, and commits changes to disk.
  virtual void SetVaultKeys(const GaiaId& gaia_id,
                            SecurityDomainId domain,
                            const std::vector<std::vector<uint8_t>>& keys,
                            int last_key_version) = 0;

  // Returns whether keys for `gaia_id` in `domain` are marked as stale by
  // consumer.
  // Returns false if user vault or domain data does not exist.
  virtual bool GetKeysMarkedAsStaleByConsumer(
      const GaiaId& gaia_id,
      SecurityDomainId domain) const = 0;

  // Sets whether keys for `gaia_id` in `domain` are marked as stale by
  // consumer.
  // Creates the user vault if it does not exist, and commits changes to disk.
  virtual void SetKeysMarkedAsStaleByConsumer(const GaiaId& gaia_id,
                                              SecurityDomainId domain,
                                              bool keys_marked_as_stale) = 0;

  // Checks whether there is any non-constant key for `gaia_id` in `domain`.
  // This indicates that the corresponding security domain is not in the
  // pre-enrollment state, but contains usable key material.
  // Returns false if the user vault does not exist or only contains constant
  // keys.
  virtual bool HasNonConstantKey(const GaiaId& gaia_id,
                                 SecurityDomainId domain) const = 0;
};

// Storage interface for request throttling state.
class ConnectionThrottlingStorage {
 public:
  virtual ~ConnectionThrottlingStorage() = default;

  // Returns the security domain-scoped time (in milliseconds since UNIX epoch)
  // at which last failed request was sent for `gaia_id` in `domain`.
  // Returns 0 if no failed requests were recorded or if the user vault does not
  // exist.
  virtual int64_t GetLastFailedRequestMillis(const GaiaId& gaia_id,
                                             SecurityDomainId domain) const = 0;

  // Sets the security domain-scoped time (in milliseconds since UNIX epoch) at
  // which last failed request was sent for `gaia_id` in `domain`.
  // Creates the user vault if it does not exist, and commits changes to disk.
  virtual void SetLastFailedRequestMillis(
      const GaiaId& gaia_id,
      SecurityDomainId domain,
      int64_t last_failed_request_millis) = 0;
};

// Storage interface for degraded recoverability state.
class DegradedRecoverabilityStorage {
 public:
  virtual ~DegradedRecoverabilityStorage() = default;

  // Returns the degraded recoverability state for `gaia_id` in `domain`.
  // Returns a default instance if the user vault or domain data does not exist.
  virtual trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
  GetDegradedRecoverabilityState(const GaiaId& gaia_id,
                                 SecurityDomainId domain) const = 0;

  // Sets the degraded recoverability state for `gaia_id` in `domain`.
  // Creates the user vault if it does not exist, and commits changes to disk.
  virtual void SetDegradedRecoverabilityState(
      const GaiaId& gaia_id,
      SecurityDomainId domain,
      const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
          state) = 0;
};

// Abstract interface for trusted vault local storage.
class StandaloneTrustedVaultStorage : public RecoveryFactorRegistrationStorage,
                                      public PhysicalDeviceStorage,
                                      public KeyStorage,
                                      public ConnectionThrottlingStorage,
                                      public DegradedRecoverabilityStorage {
 public:
  ~StandaloneTrustedVaultStorage() override = default;

  // Reads persisted state from disk.
  virtual void ReadDataFromDisk() = 0;

  // Clears all state associated with `gaia_id`.
  virtual void ClearDataForUser(const GaiaId& gaia_id) = 0;

  // Clears state for users not in `known_gaia_ids` (except
  // `primary_account_gaia_id`).
  // Preserves state for `primary_account_gaia_id` if it's not in
  // `known_gaia_ids` but marks it for deletion.
  virtual void ClearDataForUnknownUsersOrMarkForDeletion(
      const base::flat_set<GaiaId>& known_gaia_ids,
      const std::optional<GaiaId>& primary_account_gaia_id) = 0;

  // Removes users marked for deletion who are not the primary account.
  virtual void ClearDataForUsersMarkedForDeletion(
      const std::optional<GaiaId>& primary_account_gaia_id) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_

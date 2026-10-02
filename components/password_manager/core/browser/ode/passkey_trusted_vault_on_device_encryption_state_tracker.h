// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_PASSKEY_TRUSTED_VAULT_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_PASSKEY_TRUSTED_VAULT_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/trusted_vault/trusted_vault_client.h"

namespace password_manager {

// Monitors the on-device encryption state for passkeys via `TrustedVaultClient`
// of `hw_protected` security domain. Used on Android, where passkey encryption
// keys are managed via GMSCore.
// TODO(crbug.com/540854648): Refactor and consolidate the logic of tracking the
// passkey states (e.g., consider using `TrustedVaultClient` on all platforms:
// desktop, Android, and iOS).
class PasskeyTrustedVaultOnDeviceEncryptionStateTracker
    : public OnDeviceEncryptionStateTracker,
      public syncer::SyncServiceObserver,
      public trusted_vault::TrustedVaultClient::Observer {
 public:
  PasskeyTrustedVaultOnDeviceEncryptionStateTracker(
      syncer::SyncService* sync_service,
      trusted_vault::TrustedVaultClient* passkey_trusted_vault_client);

  PasskeyTrustedVaultOnDeviceEncryptionStateTracker(
      const PasskeyTrustedVaultOnDeviceEncryptionStateTracker&) = delete;
  PasskeyTrustedVaultOnDeviceEncryptionStateTracker& operator=(
      const PasskeyTrustedVaultOnDeviceEncryptionStateTracker&) = delete;

  ~PasskeyTrustedVaultOnDeviceEncryptionStateTracker() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // trusted_vault::TrustedVaultClient::Observer:
  void OnTrustedVaultKeysChanged(
      std::optional<trusted_vault::TrustedVaultUserActionTriggerForUMA> trigger)
      override;
  void OnTrustedVaultRecoverabilityChanged() override;

 private:
  void ComputeState();
  bool IsSignedInAccountChanged();
  void FetchKeysAndComputeStateIfAccountIsNotEmpty();
  void UpdateInformationAboutKeysAndComputeState(
      const std::vector<std::vector<uint8_t>>& keys);

  syncer::SyncService* sync_service();
  trusted_vault::TrustedVaultClient* passkey_trusted_vault_client();

  // Keeps account for which `FetchKeys()` is currently in flight or cached.
  CoreAccountInfo current_account_info_;

  // Cached result of `TrustedVaultClient::FetchKeys()`: `true` if non-empty
  // keys were returned, `false` if empty keys were returned, or `std::nullopt`
  // if not fetched yet.
  std::optional<bool> has_keys_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::ScopedObservation<trusted_vault::TrustedVaultClient,
                          trusted_vault::TrustedVaultClient::Observer>
      trusted_vault_client_observation_{this};

  // Used to cancel in-flight `FetchKeys()` callbacks when the account changes
  // or when keys change.
  base::WeakPtrFactory<PasskeyTrustedVaultOnDeviceEncryptionStateTracker>
      fetch_keys_weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_PASSKEY_TRUSTED_VAULT_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_

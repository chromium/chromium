// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_PASSWORD_TRUSTED_VAULT_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_PASSWORD_TRUSTED_VAULT_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_

#include "base/scoped_observation.h"
#include "components/password_manager/core/browser/ode/on_device_encryption_state_tracker.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace password_manager {

// Monitors the on-device encryption state for passwords. Tracks whether
// passwords are encrypted via Trusted Vault and whether the encryption
// key is ready/available on the device.
class PasswordTrustedVaultOnDeviceEncryptionStateTracker
    : public OnDeviceEncryptionStateTracker,
      public syncer::SyncServiceObserver {
 public:
  explicit PasswordTrustedVaultOnDeviceEncryptionStateTracker(
      syncer::SyncService* sync_service);

  PasswordTrustedVaultOnDeviceEncryptionStateTracker(
      const PasswordTrustedVaultOnDeviceEncryptionStateTracker&) = delete;
  PasswordTrustedVaultOnDeviceEncryptionStateTracker& operator=(
      const PasswordTrustedVaultOnDeviceEncryptionStateTracker&) = delete;

  ~PasswordTrustedVaultOnDeviceEncryptionStateTracker() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  // Computes the on-device encryption state at the current point in time, and
  // passes it to `OnDeviceEncryptionStateTracker::SetState` (which will cache
  // the current state and will notify observers if the state has changed).
  void ComputeState();

  syncer::SyncService* sync_service();

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ODE_PASSWORD_TRUSTED_VAULT_ON_DEVICE_ENCRYPTION_STATE_TRACKER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ode/passkey_trusted_vault_on_device_encryption_state_tracker.h"

#include "base/functional/bind.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace password_manager {

PasskeyTrustedVaultOnDeviceEncryptionStateTracker::
    PasskeyTrustedVaultOnDeviceEncryptionStateTracker(
        syncer::SyncService* sync_service,
        trusted_vault::TrustedVaultClient* passkey_trusted_vault_client) {
  // TODO(crbug.com/540854648): Consider `CHECK`-ing that `sync_service` and
  // `passkey_trusted_vault_client` are non-null (and handling null services in
  // the factory instead).
  if (sync_service) {
    sync_service_observation_.Observe(sync_service);
  }
  if (passkey_trusted_vault_client) {
    trusted_vault_client_observation_.Observe(passkey_trusted_vault_client);
  }
  FetchKeysAndComputeStateIfAccountIsNotEmpty();
}

PasskeyTrustedVaultOnDeviceEncryptionStateTracker::
    ~PasskeyTrustedVaultOnDeviceEncryptionStateTracker() = default;

void PasskeyTrustedVaultOnDeviceEncryptionStateTracker::OnStateChanged(
    syncer::SyncService* sync) {
  if (IsSignedInAccountChanged()) {
    FetchKeysAndComputeStateIfAccountIsNotEmpty();
  } else {
    ComputeState();
  }
}

void PasskeyTrustedVaultOnDeviceEncryptionStateTracker::OnSyncShutdown(
    syncer::SyncService* sync) {
  sync_service_observation_.Reset();
  trusted_vault_client_observation_.Reset();
  ComputeState();
}

void PasskeyTrustedVaultOnDeviceEncryptionStateTracker::
    OnTrustedVaultKeysChanged(
        std::optional<trusted_vault::TrustedVaultUserActionTriggerForUMA>
            trigger) {
  FetchKeysAndComputeStateIfAccountIsNotEmpty();
}

void PasskeyTrustedVaultOnDeviceEncryptionStateTracker::
    OnTrustedVaultRecoverabilityChanged() {}

void PasskeyTrustedVaultOnDeviceEncryptionStateTracker::ComputeState() {
  // The logic of deriving the state based on sync_service() is similar for
  // passkey state tracker and for password state tracker.
  // TODO(crbug.com/540854648): Consider moving the common logic to a base
  // class.
  if (!sync_service() || !passkey_trusted_vault_client()) {
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
    return;
  }
  if (sync_service()->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN) ||
      sync_service()->GetAccountInfo().IsEmpty()) {
    SetState(OnDeviceEncryptionState::kProfileNotSignedIn);
    return;
  }
  if (sync_service()->GetTransportState() ==
      syncer::SyncService::TransportState::PAUSED) {
    SetState(OnDeviceEncryptionState::kProfileSignInPending);
    return;
  }
  if (!sync_service()->IsEngineInitialized()) {
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
    return;
  }

  syncer::SyncUserSettings* user_settings = sync_service()->GetUserSettings();
  if (!user_settings || !user_settings->GetSelectedTypes().Has(
                            syncer::UserSelectableType::kPasswords)) {
    // TODO(crbug.com/540854648): Consider introducing separate states for
    // cases when sync is disabled by a user or by an enterprise policy.
    SetState(OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled);
    return;
  }

  if (!has_keys_.has_value()) {
    SetState(OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
    return;
  }

  // TODO(crbug.com/540854648): `TrustedVaultClient::FetchKeys()` returns an
  // empty key list both when the security domain has not been initialized and
  // when keys are not locally available on the device. Consider distinguishing
  // `kOnDeviceEncryptionNotEnabled` from `kDeviceNotReady`.
  SetState(*has_keys_ ? OnDeviceEncryptionState::kDeviceReady
                      : OnDeviceEncryptionState::kDeviceNotReady);
}

bool PasskeyTrustedVaultOnDeviceEncryptionStateTracker::
    IsSignedInAccountChanged() {
  CoreAccountInfo new_account_info =
      sync_service() ? sync_service()->GetAccountInfo() : CoreAccountInfo();
  return current_account_info_ != new_account_info;
}

void PasskeyTrustedVaultOnDeviceEncryptionStateTracker::
    FetchKeysAndComputeStateIfAccountIsNotEmpty() {
  fetch_keys_weak_ptr_factory_.InvalidateWeakPtrs();
  has_keys_.reset();
  current_account_info_ =
      sync_service() ? sync_service()->GetAccountInfo() : CoreAccountInfo();
  ComputeState();

  if (!passkey_trusted_vault_client() || current_account_info_.IsEmpty()) {
    return;
  }

  passkey_trusted_vault_client()->FetchKeys(
      current_account_info_,
      base::BindOnce(&PasskeyTrustedVaultOnDeviceEncryptionStateTracker::
                         UpdateInformationAboutKeysAndComputeState,
                     fetch_keys_weak_ptr_factory_.GetWeakPtr()));
}

void PasskeyTrustedVaultOnDeviceEncryptionStateTracker::
    UpdateInformationAboutKeysAndComputeState(
        const std::vector<std::vector<uint8_t>>& keys) {
  has_keys_ = !keys.empty();
  ComputeState();
}

syncer::SyncService*
PasskeyTrustedVaultOnDeviceEncryptionStateTracker::sync_service() {
  return sync_service_observation_.GetSource();
}

trusted_vault::TrustedVaultClient*
PasskeyTrustedVaultOnDeviceEncryptionStateTracker::
    passkey_trusted_vault_client() {
  return trusted_vault_client_observation_.GetSource();
}

}  // namespace password_manager

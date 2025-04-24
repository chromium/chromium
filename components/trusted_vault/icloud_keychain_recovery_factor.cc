// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/icloud_keychain_recovery_factor.h"

#include "base/task/bind_post_task.h"
#include "components/trusted_vault/trusted_vault_histograms.h"

namespace trusted_vault {

ICloudKeychainRecoveryFactor::ICloudKeychainRecoveryFactor(
    StandaloneTrustedVaultStorage* storage,
    std::optional<CoreAccountInfo> primary_account)
    : storage_(storage), primary_account_(primary_account) {
  CHECK(storage_);
}
ICloudKeychainRecoveryFactor::~ICloudKeychainRecoveryFactor() = default;

LocalRecoveryFactorType ICloudKeychainRecoveryFactor::GetRecoveryFactorType()
    const {
  return LocalRecoveryFactorType::kICloudKeychain;
}

void ICloudKeychainRecoveryFactor::AttemptRecovery(
    TrustedVaultThrottlingConnection* connection,
    AttemptRecoveryCallback cb,
    AttemptRecoveryFailureCallback failure_cb) {
  NOTIMPLEMENTED();
}

bool ICloudKeychainRecoveryFactor::IsRegistered() {
  NOTIMPLEMENTED();
  return false;
}

void ICloudKeychainRecoveryFactor::MarkAsNotRegistered() {
  NOTIMPLEMENTED();
}

void ICloudKeychainRecoveryFactor::ClearRegistrationAttemptInfo(
    const GaiaId& gaia_id) {
  NOTIMPLEMENTED();
}

TrustedVaultDeviceRegistrationStateForUMA
ICloudKeychainRecoveryFactor::MaybeRegister(
    TrustedVaultThrottlingConnection* connection,
    RegisterCallback cb) {
  NOTIMPLEMENTED();
  return TrustedVaultDeviceRegistrationStateForUMA::kLocalKeysAreStale;
}

}  // namespace trusted_vault

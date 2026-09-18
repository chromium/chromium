// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/legacy_standalone_trusted_vault_storage_adapter.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/trusted_vault/legacy_standalone_trusted_vault_storage.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

namespace {

constexpr int kCurrentDeviceRegistrationVersion = 1;

}  // namespace

LegacyStandaloneTrustedVaultStorageAdapter::
    LegacyStandaloneTrustedVaultStorageAdapter(
        std::unique_ptr<LegacyStandaloneTrustedVaultStorage> storage)
    : storage_(std::move(storage)) {
  CHECK(storage_);
}

LegacyStandaloneTrustedVaultStorageAdapter::
    ~LegacyStandaloneTrustedVaultStorageAdapter() = default;

void LegacyStandaloneTrustedVaultStorageAdapter::ReadDataFromDisk() {
  storage_->ReadDataFromDisk();
}

void LegacyStandaloneTrustedVaultStorageAdapter::ClearDataForUser(
    const GaiaId& gaia_id) {
  storage_->RemoveUserVaults([&](const UserVault& vault) {
    return vault.gaia_id() == gaia_id.ToString();
  });
}

void LegacyStandaloneTrustedVaultStorageAdapter::
    ClearDataForUnknownUsersOrMarkForDeletion(
        const base::flat_set<GaiaId>& known_gaia_ids,
        const std::optional<GaiaId>& primary_account_gaia_id) {
  // Primary account data shouldn't be removed immediately, but it needs to be
  // removed once account become non-primary if it was ever removed from cookie
  // jar (or known Gaia IDs).
  if (primary_account_gaia_id.has_value() &&
      !known_gaia_ids.contains(*primary_account_gaia_id)) {
    storage_->MutateUserVault(
        *primary_account_gaia_id, [](UserVault& user_vault) {
          user_vault.set_should_delete_keys_when_non_primary(true);
        });
  }

  storage_->RemoveUserVaults([&](const UserVault& vault) {
    const GaiaId gaia_id(vault.gaia_id());
    if (primary_account_gaia_id.has_value() &&
        gaia_id == *primary_account_gaia_id) {
      // Don't delete primary account data.
      return false;
    }
    // Delete data if account isn't in known Gaia IDs.
    return !known_gaia_ids.contains(gaia_id);
  });
}

void LegacyStandaloneTrustedVaultStorageAdapter::
    ClearDataForUsersMarkedForDeletion(
        const std::optional<GaiaId>& primary_account_gaia_id) {
  storage_->RemoveUserVaults([&](const UserVault& vault) {
    return vault.should_delete_keys_when_non_primary() &&
           (!primary_account_gaia_id.has_value() ||
            *primary_account_gaia_id != GaiaId(vault.gaia_id()));
  });
}

bool LegacyStandaloneTrustedVaultStorageAdapter::IsRecoveryFactorRegistered(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    LocalRecoveryFactorType factor_type) const {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  const UserVault* user_vault = storage_->FindUserVault(gaia_id);
  if (!user_vault) {
    return false;
  }
  switch (factor_type) {
    case LocalRecoveryFactorType::kPhysicalDevice:
      return user_vault->local_device_registration_info().device_registered();
#if BUILDFLAG(IS_MAC)
    case LocalRecoveryFactorType::kICloudKeychain:
      return user_vault->icloud_keychain_registration_info().registered();
#endif
  }
  NOTREACHED();
}

void LegacyStandaloneTrustedVaultStorageAdapter::SetRecoveryFactorRegistered(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    LocalRecoveryFactorType factor_type,
    bool registered) {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  switch (factor_type) {
    case LocalRecoveryFactorType::kPhysicalDevice:
      storage_->MutateLocalDeviceRegistrationInfo(
          gaia_id, [&](LocalDeviceRegistrationInfo& info) {
            info.set_device_registered(registered);
            if (registered) {
              info.set_device_registered_version(
                  kCurrentDeviceRegistrationVersion);
            } else {
              info.clear_device_registered_version();
            }
          });
      return;
#if BUILDFLAG(IS_MAC)
    case LocalRecoveryFactorType::kICloudKeychain:
      storage_->MutateICloudKeychainRegistrationInfo(
          gaia_id, [&](ICloudKeychainRegistrationInfo& info) {
            info.set_registered(registered);
          });
      return;
#endif
  }
  NOTREACHED();
}

bool LegacyStandaloneTrustedVaultStorageAdapter::
    GetLastRegistrationReturnedLocalDataObsolete(
        const GaiaId& gaia_id,
        SecurityDomainId domain) const {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  return storage_->GetLastRegistrationReturnedLocalDataObsolete(gaia_id);
}

void LegacyStandaloneTrustedVaultStorageAdapter::
    SetLastRegistrationReturnedLocalDataObsolete(const GaiaId& gaia_id,
                                                 SecurityDomainId domain,
                                                 bool obsolete) {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  storage_->SetLastRegistrationReturnedLocalDataObsolete(gaia_id, obsolete);
}

const LocalDeviceRegistrationInfo&
LegacyStandaloneTrustedVaultStorageAdapter::GetLocalDeviceRegistrationInfo(
    const GaiaId& gaia_id) const {
  const UserVault* user_vault = storage_->FindUserVault(gaia_id);
  if (!user_vault) {
    return LocalDeviceRegistrationInfo::default_instance();
  }
  return user_vault->local_device_registration_info();
}

void LegacyStandaloneTrustedVaultStorageAdapter::
    MutateLocalDeviceRegistrationInfo(
        const GaiaId& gaia_id,
        base::FunctionRef<void(LocalDeviceRegistrationInfo&)> mutator) {
  storage_->MutateLocalDeviceRegistrationInfo(gaia_id, mutator);
}

std::vector<std::vector<uint8_t>>
LegacyStandaloneTrustedVaultStorageAdapter::GetVaultKeys(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  return storage_->GetVaultKeys(gaia_id);
}

int LegacyStandaloneTrustedVaultStorageAdapter::GetLastKeyVersion(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  return storage_->GetLastKeyVersion(gaia_id);
}

void LegacyStandaloneTrustedVaultStorageAdapter::SetVaultKeys(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  storage_->SetVaultKeys(gaia_id, keys, last_key_version);
}

bool LegacyStandaloneTrustedVaultStorageAdapter::GetKeysMarkedAsStaleByConsumer(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  return storage_->GetKeysMarkedAsStaleByConsumer(gaia_id);
}

void LegacyStandaloneTrustedVaultStorageAdapter::SetKeysMarkedAsStaleByConsumer(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    bool stale) {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  storage_->SetKeysMarkedAsStaleByConsumer(gaia_id, stale);
}

bool LegacyStandaloneTrustedVaultStorageAdapter::HasNonConstantKey(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  return storage_->HasNonConstantKey(gaia_id);
}

int64_t LegacyStandaloneTrustedVaultStorageAdapter::GetLastFailedRequestMillis(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  return storage_->GetLastFailedRequestMillis(gaia_id);
}

void LegacyStandaloneTrustedVaultStorageAdapter::SetLastFailedRequestMillis(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    int64_t last_failed_request_millis) {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  storage_->SetLastFailedRequestMillis(gaia_id, last_failed_request_millis);
}

trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
LegacyStandaloneTrustedVaultStorageAdapter::GetDegradedRecoverabilityState(
    const GaiaId& gaia_id,
    SecurityDomainId domain) const {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  const UserVault* user_vault = storage_->FindUserVault(gaia_id);
  if (!user_vault) {
    return trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState();
  }
  return user_vault->degraded_recoverability_state();
}

void LegacyStandaloneTrustedVaultStorageAdapter::SetDegradedRecoverabilityState(
    const GaiaId& gaia_id,
    SecurityDomainId domain,
    const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
        state) {
  CHECK_EQ(domain, SecurityDomainId::kChromeSync);
  storage_->MutateUserVault(gaia_id, [&](UserVault& user_vault) {
    *user_vault.mutable_degraded_recoverability_state() = state;
  });
}

}  // namespace trusted_vault

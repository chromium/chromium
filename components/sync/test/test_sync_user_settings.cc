// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/test_sync_user_settings.h"

#include "build/chromeos_buildflags.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings_impl.h"
#include "components/sync/test/test_sync_service.h"

namespace syncer {

ModelTypeSet UserSelectableTypesToModelTypes(
    UserSelectableTypeSet selected_types) {
  ModelTypeSet preferred_types;
  for (UserSelectableType type : selected_types) {
    preferred_types.PutAll(UserSelectableTypeToAllModelTypes(type));
  }
  return preferred_types;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
ModelTypeSet UserSelectableOsTypesToModelTypes(
    UserSelectableOsTypeSet selected_types) {
  ModelTypeSet preferred_types;
  for (UserSelectableOsType type : selected_types) {
    preferred_types.PutAll(UserSelectableOsTypeToAllModelTypes(type));
  }
  return preferred_types;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TestSyncUserSettings::TestSyncUserSettings(TestSyncService* service)
    : service_(service),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      selected_os_types_(UserSelectableOsTypeSet::All()),
#endif
      selected_types_(UserSelectableTypeSet::All()) {
}

TestSyncUserSettings::~TestSyncUserSettings() = default;

bool TestSyncUserSettings::IsInitialSyncFeatureSetupComplete() const {
  return initial_sync_feature_setup_complete_;
}

void TestSyncUserSettings::SetInitialSyncFeatureSetupComplete(
    SyncFirstSetupCompleteSource source) {
  SetInitialSyncFeatureSetupComplete();
}

bool TestSyncUserSettings::IsSyncEverythingEnabled() const {
  return sync_everything_enabled_;
}

void TestSyncUserSettings::SetSelectedTypes(bool sync_everything,
                                            UserSelectableTypeSet types) {
  // TODO(crbug.com/1330894): take custom logic for Lacros apps into account.

  sync_everything_enabled_ = sync_everything;

  if (sync_everything_enabled_) {
    selected_types_.PutAll(UserSelectableTypeSet::All());
  } else {
    selected_types_ = types;
  }

  service_->FirePaymentsIntegrationEnabledChanged();
}

void TestSyncUserSettings::SetSelectedType(UserSelectableType type,
                                           bool is_type_on) {
  if (is_type_on) {
    selected_types_.Put(type);
  } else {
    selected_types_.Remove(type);
  }

  service_->FirePaymentsIntegrationEnabledChanged();
}

void TestSyncUserSettings::KeepAccountSettingsPrefsOnlyForUsers(
    const std::vector<signin::GaiaIdHash>& available_gaia_ids) {}

#if BUILDFLAG(IS_IOS)
void TestSyncUserSettings::SetBookmarksAndReadingListAccountStorageOptIn(
    bool value) {}
#endif  // BUILDFLAG(IS_IOS)

UserSelectableTypeSet TestSyncUserSettings::GetSelectedTypes() const {
  return selected_types_;
}

bool TestSyncUserSettings::IsTypeManagedByPolicy(
    UserSelectableType type) const {
  return managed_types_.Has(type);
}

bool TestSyncUserSettings::IsTypeManagedByCustodian(
    UserSelectableType type) const {
  return false;
}

ModelTypeSet TestSyncUserSettings::GetPreferredDataTypes() const {
  ModelTypeSet types = UserSelectableTypesToModelTypes(GetSelectedTypes());
  types.PutAll(AlwaysPreferredUserTypes());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  types.PutAll(UserSelectableOsTypesToModelTypes(GetSelectedOsTypes()));
#endif
  types.PutAll(ControlTypes());
  return types;
}

UserSelectableTypeSet TestSyncUserSettings::GetRegisteredSelectableTypes()
    const {
  return UserSelectableTypeSet::All();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool TestSyncUserSettings::IsSyncAllOsTypesEnabled() const {
  return sync_all_os_types_enabled_;
}

UserSelectableOsTypeSet TestSyncUserSettings::GetSelectedOsTypes() const {
  return selected_os_types_;
}

bool TestSyncUserSettings::IsOsTypeManagedByPolicy(
    UserSelectableOsType type) const {
  return managed_os_types_.Has(type);
}

void TestSyncUserSettings::SetSelectedOsTypes(bool sync_all_os_types,
                                              UserSelectableOsTypeSet types) {
  sync_all_os_types_enabled_ = sync_all_os_types;

  UserSelectableOsTypeSet selected_os_types;

  if (sync_all_os_types_enabled_) {
    for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
      selected_os_types.Put(type);
    }
  } else {
    for (UserSelectableOsType type : types) {
      selected_os_types.Put(type);
    }
  }

  selected_os_types_ = selected_os_types;
}

UserSelectableOsTypeSet TestSyncUserSettings::GetRegisteredSelectableOsTypes()
    const {
  return UserSelectableOsTypeSet::All();
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void TestSyncUserSettings::SetAppsSyncEnabledByOs(bool apps_sync_enabled) {
  UserSelectableTypeSet selected_types = GetSelectedTypes();
  if (apps_sync_enabled) {
    selected_types.Put(UserSelectableType::kApps);
  } else {
    selected_types.Remove(UserSelectableType::kApps);
  }
  SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/selected_types);
}
#endif

bool TestSyncUserSettings::IsCustomPassphraseAllowed() const {
  return true;
}

void TestSyncUserSettings::SetCustomPassphraseAllowed(bool allowed) {}

bool TestSyncUserSettings::IsEncryptEverythingEnabled() const {
  return false;
}

ModelTypeSet TestSyncUserSettings::GetEncryptedDataTypes() const {
  if (!IsUsingExplicitPassphrase()) {
    // PASSWORDS and WIFI_CONFIGURATIONS are always encrypted.
    return {PASSWORDS, WIFI_CONFIGURATIONS};
  }
  // Some types can never be encrypted, e.g. DEVICE_INFO and
  // AUTOFILL_WALLET_DATA, so make sure we don't report them as encrypted.
  return Intersection(GetPreferredDataTypes(), EncryptableUserTypes());
}

bool TestSyncUserSettings::IsPassphraseRequired() const {
  return passphrase_required_;
}

bool TestSyncUserSettings::IsPassphraseRequiredForPreferredDataTypes() const {
  return passphrase_required_for_preferred_data_types_;
}

bool TestSyncUserSettings::IsPassphrasePromptMutedForCurrentProductVersion()
    const {
  return false;
}

void TestSyncUserSettings::MarkPassphrasePromptMutedForCurrentProductVersion() {
}

bool TestSyncUserSettings::IsTrustedVaultKeyRequired() const {
  return trusted_vault_key_required_;
}

bool TestSyncUserSettings::IsTrustedVaultKeyRequiredForPreferredDataTypes()
    const {
  return trusted_vault_key_required_for_preferred_data_types_;
}

bool TestSyncUserSettings::IsTrustedVaultRecoverabilityDegraded() const {
  return trusted_vault_recoverability_degraded_;
}

bool TestSyncUserSettings::IsUsingExplicitPassphrase() const {
  return using_explicit_passphrase_;
}

base::Time TestSyncUserSettings::GetExplicitPassphraseTime() const {
  return base::Time();
}

absl::optional<PassphraseType> TestSyncUserSettings::GetPassphraseType() const {
  return IsUsingExplicitPassphrase() ? PassphraseType::kCustomPassphrase
                                     : PassphraseType::kImplicitPassphrase;
}

void TestSyncUserSettings::SetEncryptionPassphrase(
    const std::string& passphrase) {}

bool TestSyncUserSettings::SetDecryptionPassphrase(
    const std::string& passphrase) {
  return false;
}

void TestSyncUserSettings::SetDecryptionNigoriKey(
    std::unique_ptr<Nigori> nigori) {}

std::unique_ptr<Nigori> TestSyncUserSettings::GetDecryptionNigoriKey() const {
  return nullptr;
}

void TestSyncUserSettings::SetInitialSyncFeatureSetupComplete() {
  initial_sync_feature_setup_complete_ = true;
}

void TestSyncUserSettings::ClearInitialSyncFeatureSetupComplete() {
  initial_sync_feature_setup_complete_ = false;
}

void TestSyncUserSettings::SetTypeIsManaged(UserSelectableType type,
                                            bool managed) {
  if (managed) {
    managed_types_.Put(type);
  } else {
    managed_types_.Remove(type);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void TestSyncUserSettings::SetOsTypeIsManaged(UserSelectableOsType type,
                                              bool managed) {
  if (managed) {
    managed_os_types_.Put(type);
  } else {
    managed_os_types_.Remove(type);
  }
}
#endif

void TestSyncUserSettings::SetPassphraseRequired(bool required) {
  passphrase_required_ = required;
}

void TestSyncUserSettings::SetPassphraseRequiredForPreferredDataTypes(
    bool required) {
  passphrase_required_for_preferred_data_types_ = required;
}

void TestSyncUserSettings::SetTrustedVaultKeyRequired(bool required) {
  trusted_vault_key_required_ = required;
}

void TestSyncUserSettings::SetTrustedVaultKeyRequiredForPreferredDataTypes(
    bool required) {
  trusted_vault_key_required_for_preferred_data_types_ = required;
}

void TestSyncUserSettings::SetTrustedVaultRecoverabilityDegraded(
    bool degraded) {
  trusted_vault_recoverability_degraded_ = degraded;
}

void TestSyncUserSettings::SetIsUsingExplicitPassphrase(bool enabled) {
  using_explicit_passphrase_ = enabled;
}

}  // namespace syncer

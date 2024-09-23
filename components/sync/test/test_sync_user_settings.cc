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
namespace {

const char kDefaultPassphrase[] = "TestPassphrase";

}  // namespace

DataTypeSet UserSelectableTypesToDataTypes(
    UserSelectableTypeSet selected_types) {
  DataTypeSet preferred_types;
  for (UserSelectableType type : selected_types) {
    preferred_types.PutAll(UserSelectableTypeToAllDataTypes(type));
  }
  return preferred_types;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
DataTypeSet UserSelectableOsTypesToDataTypes(
    UserSelectableOsTypeSet selected_types) {
  DataTypeSet preferred_types;
  for (UserSelectableOsType type : selected_types) {
    preferred_types.PutAll(UserSelectableOsTypeToAllDataTypes(type));
  }
  return preferred_types;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TestSyncUserSettings::TestSyncUserSettings(TestSyncService* service)
    : service_(service) {}

TestSyncUserSettings::~TestSyncUserSettings() = default;

bool TestSyncUserSettings::IsInitialSyncFeatureSetupComplete() const {
  return initial_sync_feature_setup_complete_;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void TestSyncUserSettings::SetInitialSyncFeatureSetupComplete(
    SyncFirstSetupCompleteSource source) {
  SetInitialSyncFeatureSetupComplete();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

bool TestSyncUserSettings::IsSyncEverythingEnabled() const {
  return sync_everything_enabled_;
}

void TestSyncUserSettings::SetSelectedTypes(bool sync_everything,
                                            UserSelectableTypeSet types) {
  // TODO(crbug.com/40227318): take custom logic for Lacros apps into account.

  sync_everything_enabled_ = sync_everything;

  if (sync_everything_enabled_) {
    selected_types_.PutAll(UserSelectableTypeSet::All());
  } else {
    selected_types_ = types;
  }
}

void TestSyncUserSettings::SetSelectedType(UserSelectableType type,
                                           bool is_type_on) {
  if (is_type_on) {
    selected_types_.Put(type);
  } else {
    selected_types_.Remove(type);
  }
}

void TestSyncUserSettings::KeepAccountSettingsPrefsOnlyForUsers(
    const std::vector<signin::GaiaIdHash>& available_gaia_ids) {}

UserSelectableTypeSet TestSyncUserSettings::GetSelectedTypes() const {
  if (service_->GetAccountInfo().IsEmpty()) {
    return {};
  }
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

SyncUserSettings::UserSelectableTypePrefState
TestSyncUserSettings::GetTypePrefStateForAccount(
    UserSelectableType type) const {
  if (selected_types_.Has(type)) {
    return SyncUserSettings::UserSelectableTypePrefState::kEnabledOrDefault;
  }
  return SyncUserSettings::UserSelectableTypePrefState::kDisabled;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
int TestSyncUserSettings::GetNumberOfAccountsWithPasswordsSelected() const {
  return selected_types_.Has(UserSelectableType::kPasswords) ? 1 : 0;
}
#endif

DataTypeSet TestSyncUserSettings::GetPreferredDataTypes() const {
  DataTypeSet types = UserSelectableTypesToDataTypes(GetSelectedTypes());
  types.PutAll(AlwaysPreferredUserTypes());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  types.PutAll(UserSelectableOsTypesToDataTypes(GetSelectedOsTypes()));
#endif
  types.PutAll(ControlTypes());
  return types;
}

UserSelectableTypeSet TestSyncUserSettings::GetRegisteredSelectableTypes()
    const {
  return registered_selectable_types_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool TestSyncUserSettings::IsSyncFeatureDisabledViaDashboard() const {
  return sync_feature_disabled_via_dashboard_;
}

void TestSyncUserSettings::SetSyncFeatureDisabledViaDashboard(
    bool disabled_via_dashboard) {
  sync_feature_disabled_via_dashboard_ = disabled_via_dashboard;
}

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
  return custom_passphrase_allowed_;
}

void TestSyncUserSettings::SetCustomPassphraseAllowed(bool allowed) {
  custom_passphrase_allowed_ = allowed;
}

bool TestSyncUserSettings::IsEncryptEverythingEnabled() const {
  return IsExplicitPassphrase(passphrase_type_);
}

DataTypeSet TestSyncUserSettings::GetAllEncryptedDataTypes() const {
  return IsUsingExplicitPassphrase() ? EncryptableUserTypes()
                                     : AlwaysEncryptedUserTypes();
}

bool TestSyncUserSettings::IsPassphraseRequired() const {
  return passphrase_required_;
}

bool TestSyncUserSettings::IsPassphraseRequiredForPreferredDataTypes() const {
  return IsPassphraseRequired() && IsEncryptedDatatypePreferred();
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
  return IsTrustedVaultKeyRequired() && IsEncryptedDatatypePreferred();
}

bool TestSyncUserSettings::IsTrustedVaultRecoverabilityDegraded() const {
  return trusted_vault_recoverability_degraded_;
}

bool TestSyncUserSettings::IsUsingExplicitPassphrase() const {
  return IsExplicitPassphrase(passphrase_type_);
}

base::Time TestSyncUserSettings::GetExplicitPassphraseTime() const {
  return explicit_passphrase_time_;
}

std::optional<PassphraseType> TestSyncUserSettings::GetPassphraseType() const {
  return passphrase_type_;
}

void TestSyncUserSettings::SetEncryptionPassphrase(
    const std::string& passphrase) {
  encryption_passphrase_ = passphrase;
  SetIsUsingExplicitPassphrase(true);
}

bool TestSyncUserSettings::SetDecryptionPassphrase(
    const std::string& passphrase) {
  if (passphrase.empty() || passphrase != encryption_passphrase_) {
    return false;
  }

  passphrase_required_ = false;
  return true;
}

void TestSyncUserSettings::SetExplicitPassphraseDecryptionNigoriKey(
    std::unique_ptr<Nigori> nigori) {}

std::unique_ptr<Nigori>
TestSyncUserSettings::GetExplicitPassphraseDecryptionNigoriKey() const {
  return nullptr;
}

void TestSyncUserSettings::SetRegisteredSelectableTypes(
    UserSelectableTypeSet types) {
  registered_selectable_types_ = types;
  selected_types_ = Intersection(selected_types_, types);
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

void TestSyncUserSettings::SetPassphraseRequired() {
  SetPassphraseRequired(kDefaultPassphrase);
}

void TestSyncUserSettings::SetPassphraseRequired(
    const std::string& passphrase) {
  CHECK(!passphrase.empty());
  encryption_passphrase_ = passphrase;
  passphrase_required_ = true;
}

void TestSyncUserSettings::SetTrustedVaultKeyRequired(bool required) {
  trusted_vault_key_required_ = required;
}

void TestSyncUserSettings::SetTrustedVaultRecoverabilityDegraded(
    bool degraded) {
  trusted_vault_recoverability_degraded_ = degraded;
}

void TestSyncUserSettings::SetIsUsingExplicitPassphrase(bool enabled) {
  SetPassphraseType(enabled ? PassphraseType::kCustomPassphrase
                            : PassphraseType::kKeystorePassphrase);
}

void TestSyncUserSettings::SetPassphraseType(PassphraseType type) {
  CHECK(custom_passphrase_allowed_ || !IsExplicitPassphrase(type));
  passphrase_type_ = type;
}

void TestSyncUserSettings::SetExplicitPassphraseTime(base::Time t) {
  CHECK(IsUsingExplicitPassphrase());
  explicit_passphrase_time_ = t;
}

const std::string& TestSyncUserSettings::GetEncryptionPassphrase() const {
  return encryption_passphrase_;
}

bool TestSyncUserSettings::IsEncryptedDatatypePreferred() const {
  return !Intersection(GetPreferredDataTypes(), GetAllEncryptedDataTypes())
              .empty();
}

}  // namespace syncer

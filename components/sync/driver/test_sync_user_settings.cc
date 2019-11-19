// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/test_sync_user_settings.h"

#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings_impl.h"
#include "components/sync/driver/test_sync_service.h"

namespace syncer {

TestSyncUserSettings::TestSyncUserSettings(TestSyncService* service)
    : service_(service) {}

TestSyncUserSettings::~TestSyncUserSettings() = default;

bool TestSyncUserSettings::IsSyncRequested() const {
  return !service_->HasDisableReason(SyncService::DISABLE_REASON_USER_CHOICE);
}

void TestSyncUserSettings::SetSyncRequested(bool requested) {
  int disable_reasons = service_->GetDisableReasons();
  if (requested) {
    disable_reasons &= ~SyncService::DISABLE_REASON_USER_CHOICE;
  } else {
    disable_reasons |= SyncService::DISABLE_REASON_USER_CHOICE;
  }
  service_->SetDisableReasons(disable_reasons);
}

bool TestSyncUserSettings::IsSyncAllowedByPlatform() const {
  return !service_->HasDisableReason(
      SyncService::DISABLE_REASON_PLATFORM_OVERRIDE);
}

void TestSyncUserSettings::SetSyncAllowedByPlatform(bool allowed) {
  int disable_reasons = service_->GetDisableReasons();
  if (allowed) {
    disable_reasons &= ~SyncService::DISABLE_REASON_PLATFORM_OVERRIDE;
  } else {
    disable_reasons |= SyncService::DISABLE_REASON_PLATFORM_OVERRIDE;
  }
  service_->SetDisableReasons(disable_reasons);
}

bool TestSyncUserSettings::IsFirstSetupComplete() const {
  return first_setup_complete_;
}

void TestSyncUserSettings::SetFirstSetupComplete(
    SyncFirstSetupCompleteSource source) {
  SetFirstSetupComplete();
}

bool TestSyncUserSettings::IsSyncEverythingEnabled() const {
  return sync_everything_enabled_;
}

UserSelectableTypeSet TestSyncUserSettings::GetSelectedTypes() const {
  // TODO(crbug.com/950874): consider getting rid of the logic inversion here.
  // service_.preferred_type should be derived from selected types, not vice
  // versa.
  ModelTypeSet preferred_types = service_->GetPreferredDataTypes();
  UserSelectableTypeSet selected_types;
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    if (preferred_types.Has(UserSelectableTypeToCanonicalModelType(type))) {
      selected_types.Put(type);
    }
  }
  return selected_types;
}

void TestSyncUserSettings::SetSelectedTypes(bool sync_everything,
                                            UserSelectableTypeSet types) {
  sync_everything_enabled_ = sync_everything;
  syncer::ModelTypeSet preferred_types;
  if (sync_everything_enabled_) {
    preferred_types = syncer::ModelTypeSet::All();
  } else {
    preferred_types =
        syncer::SyncUserSettingsImpl::ResolvePreferredTypesForTesting(types);
  }
  service_->SetPreferredDataTypes(preferred_types);
}

UserSelectableTypeSet TestSyncUserSettings::GetRegisteredSelectableTypes()
    const {
  return UserSelectableTypeSet::All();
}

UserSelectableTypeSet TestSyncUserSettings::GetForcedTypes() const {
  return {};
}

#if defined(OS_CHROMEOS)
bool TestSyncUserSettings::IsSyncAllOsTypesEnabled() const {
  return sync_all_os_types_enabled_;
}

UserSelectableOsTypeSet TestSyncUserSettings::GetSelectedOsTypes() const {
  ModelTypeSet preferred_types = service_->GetPreferredDataTypes();
  UserSelectableOsTypeSet selected_types;
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    if (preferred_types.Has(UserSelectableOsTypeToCanonicalModelType(type))) {
      selected_types.Put(type);
    }
  }
  return selected_types;
}

void TestSyncUserSettings::SetSelectedOsTypes(bool sync_all_os_types,
                                              UserSelectableOsTypeSet types) {
  sync_all_os_types_enabled_ = sync_all_os_types;

  syncer::ModelTypeSet preferred_types;
  if (sync_all_os_types_enabled_) {
    for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
      preferred_types.PutAll(UserSelectableOsTypeToAllModelTypes(type));
    }
  } else {
    for (UserSelectableOsType type : types) {
      preferred_types.PutAll(UserSelectableOsTypeToAllModelTypes(type));
    }
  }
  service_->SetPreferredDataTypes(preferred_types);
}

UserSelectableOsTypeSet TestSyncUserSettings::GetRegisteredSelectableOsTypes()
    const {
  return UserSelectableOsTypeSet::All();
}

bool TestSyncUserSettings::GetOsSyncFeatureEnabled() const {
  return os_sync_feature_enabled_;
}

void TestSyncUserSettings::SetOsSyncFeatureEnabled(bool enabled) {
  os_sync_feature_enabled_ = enabled;
}
#endif

bool TestSyncUserSettings::IsEncryptEverythingAllowed() const {
  return true;
}

void TestSyncUserSettings::SetEncryptEverythingAllowed(bool allowed) {}

bool TestSyncUserSettings::IsEncryptEverythingEnabled() const {
  return false;
}

void TestSyncUserSettings::EnableEncryptEverything() {}

ModelTypeSet TestSyncUserSettings::GetEncryptedDataTypes() const {
  if (!IsUsingSecondaryPassphrase()) {
    // PASSWORDS and WIFI_CONFIGURATIONS are always encrypted.
    return ModelTypeSet(PASSWORDS, WIFI_CONFIGURATIONS);
  }
  // Some types can never be encrypted, e.g. DEVICE_INFO and
  // AUTOFILL_WALLET_DATA, so make sure we don't report them as encrypted.
  return Intersection(service_->GetPreferredDataTypes(),
                      EncryptableUserTypes());
}

bool TestSyncUserSettings::IsPassphraseRequired() const {
  return passphrase_required_;
}

bool TestSyncUserSettings::IsPassphraseRequiredForPreferredDataTypes() const {
  return passphrase_required_for_preferred_data_types_;
}

bool TestSyncUserSettings::IsTrustedVaultKeyRequiredForPreferredDataTypes()
    const {
  return trusted_vault_key_required_for_preferred_data_types_;
}

bool TestSyncUserSettings::IsUsingSecondaryPassphrase() const {
  return using_secondary_passphrase_;
}

base::Time TestSyncUserSettings::GetExplicitPassphraseTime() const {
  return base::Time();
}

PassphraseType TestSyncUserSettings::GetPassphraseType() const {
  return IsUsingSecondaryPassphrase() ? PassphraseType::kCustomPassphrase
                                      : PassphraseType::kImplicitPassphrase;
}

void TestSyncUserSettings::SetEncryptionPassphrase(
    const std::string& passphrase) {}

bool TestSyncUserSettings::SetDecryptionPassphrase(
    const std::string& passphrase) {
  return false;
}

void TestSyncUserSettings::AddTrustedVaultDecryptionKeys(
    const std::string& gaia_id,
    const std::vector<std::string>& keys) {}

void TestSyncUserSettings::SetFirstSetupComplete() {
  first_setup_complete_ = true;
}

void TestSyncUserSettings::ClearFirstSetupComplete() {
  first_setup_complete_ = false;
}

void TestSyncUserSettings::SetPassphraseRequired(bool required) {
  passphrase_required_ = required;
}

void TestSyncUserSettings::SetPassphraseRequiredForPreferredDataTypes(
    bool required) {
  passphrase_required_for_preferred_data_types_ = required;
}

void TestSyncUserSettings::SetTrustedVaultKeyRequiredForPreferredDataTypes(
    bool required) {
  trusted_vault_key_required_for_preferred_data_types_ = required;
}

void TestSyncUserSettings::SetIsUsingSecondaryPassphrase(bool enabled) {
  using_secondary_passphrase_ = enabled;
}

}  // namespace syncer

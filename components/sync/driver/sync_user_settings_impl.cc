// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_user_settings_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/features.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service_crypto.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace syncer {

namespace {

// Converts |selected_types| to the corresponding ModelTypeSet (e.g.
// {kExtensions} becomes {EXTENSIONS, EXTENSION_SETTINGS}).
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

int GetCurrentMajorProductVersion() {
  DCHECK(version_info::GetVersion().IsValid());
  return version_info::GetVersion().components()[0];
}

}  // namespace

SyncUserSettingsImpl::SyncUserSettingsImpl(
    SyncServiceCrypto* crypto,
    SyncPrefs* prefs,
    const SyncTypePreferenceProvider* preference_provider,
    ModelTypeSet registered_model_types)
    : crypto_(crypto),
      prefs_(prefs),
      preference_provider_(preference_provider),
      registered_model_types_(registered_model_types) {
  DCHECK(crypto_);
  DCHECK(prefs_);
}

SyncUserSettingsImpl::~SyncUserSettingsImpl() = default;

bool SyncUserSettingsImpl::IsFirstSetupComplete() const {
  return prefs_->IsFirstSetupComplete();
}

void SyncUserSettingsImpl::SetFirstSetupComplete(
    SyncFirstSetupCompleteSource source) {
  if (IsFirstSetupComplete())
    return;
  UMA_HISTOGRAM_ENUMERATION("Signin.SyncFirstSetupCompleteSource", source);
  prefs_->SetFirstSetupComplete();
}

bool SyncUserSettingsImpl::IsSyncEverythingEnabled() const {
  return prefs_->HasKeepEverythingSynced();
}

UserSelectableTypeSet SyncUserSettingsImpl::GetSelectedTypes() const {
  UserSelectableTypeSet types = prefs_->GetSelectedTypes();
  types.RetainAll(GetRegisteredSelectableTypes());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (base::FeatureList::IsEnabled(kSyncChromeOSAppsToggleSharing) &&
      GetRegisteredSelectableTypes().Has(UserSelectableType::kApps)) {
    // Apps sync is controlled by dedicated preference on Lacros, corresponding
    // to Apps toggle in OS Sync settings.
    types.Remove(UserSelectableType::kApps);
    if (prefs_->IsAppsSyncEnabledByOs()) {
      types.Put(UserSelectableType::kApps);
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  return types;
}

bool SyncUserSettingsImpl::IsTypeManagedByPolicy(
    UserSelectableType type) const {
  return prefs_->IsTypeManagedByPolicy(type);
}

void SyncUserSettingsImpl::SetSelectedTypes(bool sync_everything,
                                            UserSelectableTypeSet types) {
  UserSelectableTypeSet registered_types = GetRegisteredSelectableTypes();
  DCHECK(registered_types.HasAll(types))
      << "\n registered: " << UserSelectableTypeSetToString(registered_types)
      << "\n setting to: " << UserSelectableTypeSetToString(types);
  prefs_->SetSelectedTypes(sync_everything, registered_types, types);
}

UserSelectableTypeSet SyncUserSettingsImpl::GetRegisteredSelectableTypes()
    const {
  UserSelectableTypeSet registered_types;
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    if (!base::Intersection(registered_model_types_,
                            UserSelectableTypeToAllModelTypes(type))
             .Empty()) {
      registered_types.Put(type);
    }
  }
  return registered_types;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool SyncUserSettingsImpl::IsSyncAllOsTypesEnabled() const {
  return prefs_->IsSyncAllOsTypesEnabled();
}

UserSelectableOsTypeSet SyncUserSettingsImpl::GetSelectedOsTypes() const {
  UserSelectableOsTypeSet types = prefs_->GetSelectedOsTypes();
  types.RetainAll(GetRegisteredSelectableOsTypes());
  return types;
}

bool SyncUserSettingsImpl::IsOsTypeManagedByPolicy(
    UserSelectableOsType type) const {
  return prefs_->IsOsTypeManagedByPolicy(type);
}

void SyncUserSettingsImpl::SetSelectedOsTypes(bool sync_all_os_types,
                                              UserSelectableOsTypeSet types) {
  UserSelectableOsTypeSet registered_types = GetRegisteredSelectableOsTypes();
  DCHECK(registered_types.HasAll(types));
  prefs_->SetSelectedOsTypes(sync_all_os_types, registered_types, types);
}

UserSelectableOsTypeSet SyncUserSettingsImpl::GetRegisteredSelectableOsTypes()
    const {
  UserSelectableOsTypeSet registered_types;
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    if (!base::Intersection(registered_model_types_,
                            UserSelectableOsTypeToAllModelTypes(type))
             .Empty()) {
      registered_types.Put(type);
    }
  }
  return registered_types;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void SyncUserSettingsImpl::SetAppsSyncEnabledByOs(bool apps_sync_enabled) {
  DCHECK(base::FeatureList::IsEnabled(kSyncChromeOSAppsToggleSharing));
  prefs_->SetAppsSyncEnabledByOs(apps_sync_enabled);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

bool SyncUserSettingsImpl::IsCustomPassphraseAllowed() const {
  return !preference_provider_ ||
         preference_provider_->IsCustomPassphraseAllowed();
}

bool SyncUserSettingsImpl::IsEncryptEverythingEnabled() const {
  return crypto_->IsEncryptEverythingEnabled();
}

bool SyncUserSettingsImpl::IsPassphraseRequired() const {
  return crypto_->IsPassphraseRequired();
}

bool SyncUserSettingsImpl::IsPassphraseRequiredForPreferredDataTypes() const {
  // If there is an encrypted datatype enabled and we don't have the proper
  // passphrase, we must prompt the user for a passphrase. The only way for the
  // user to avoid entering their passphrase is to disable the encrypted types.
  return IsEncryptedDatatypeEnabled() && IsPassphraseRequired();
}

bool SyncUserSettingsImpl::IsPassphrasePromptMutedForCurrentProductVersion()
    const {
  return prefs_->GetPassphrasePromptMutedProductVersion() ==
         GetCurrentMajorProductVersion();
}

void SyncUserSettingsImpl::MarkPassphrasePromptMutedForCurrentProductVersion() {
  prefs_->SetPassphrasePromptMutedProductVersion(
      GetCurrentMajorProductVersion());
}

bool SyncUserSettingsImpl::IsTrustedVaultKeyRequired() const {
  return crypto_->IsTrustedVaultKeyRequired();
}

bool SyncUserSettingsImpl::IsTrustedVaultKeyRequiredForPreferredDataTypes()
    const {
  return IsEncryptedDatatypeEnabled() && crypto_->IsTrustedVaultKeyRequired();
}

bool SyncUserSettingsImpl::IsTrustedVaultRecoverabilityDegraded() const {
  return IsEncryptedDatatypeEnabled() &&
         crypto_->IsTrustedVaultRecoverabilityDegraded();
}

bool SyncUserSettingsImpl::IsUsingExplicitPassphrase() const {
  return crypto_->IsUsingExplicitPassphrase();
}

base::Time SyncUserSettingsImpl::GetExplicitPassphraseTime() const {
  return crypto_->GetExplicitPassphraseTime();
}

PassphraseType SyncUserSettingsImpl::GetPassphraseType() const {
  return crypto_->GetPassphraseType();
}

void SyncUserSettingsImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  crypto_->SetEncryptionPassphrase(passphrase);
}

bool SyncUserSettingsImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK(IsPassphraseRequired())
      << "SetDecryptionPassphrase must not be called when "
         "IsPassphraseRequired() is false.";

  DVLOG(1) << "Setting passphrase for decryption.";

  return crypto_->SetDecryptionPassphrase(passphrase);
}

void SyncUserSettingsImpl::SetDecryptionNigoriKey(
    std::unique_ptr<Nigori> nigori) {
  return crypto_->SetDecryptionNigoriKey(std::move(nigori));
}

std::unique_ptr<Nigori> SyncUserSettingsImpl::GetDecryptionNigoriKey() const {
  return crypto_->GetDecryptionNigoriKey();
}

ModelTypeSet SyncUserSettingsImpl::GetPreferredDataTypes() const {
  ModelTypeSet types = UserSelectableTypesToModelTypes(GetSelectedTypes());
  types.PutAll(AlwaysPreferredUserTypes());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  types.PutAll(UserSelectableOsTypesToModelTypes(GetSelectedOsTypes()));
#endif
  types.RetainAll(registered_model_types_);

  static_assert(46 == GetNumModelTypes(),
                "If adding a new sync data type, update the list below below if"
                " you want to disable the new data type for local sync.");
  types.PutAll(ControlTypes());
  if (prefs_->IsLocalSyncEnabled()) {
    types.Remove(APP_LIST);
    types.Remove(AUTOFILL_WALLET_OFFER);
    types.Remove(AUTOFILL_WALLET_USAGE);
    types.Remove(CONTACT_INFO);
    types.Remove(HISTORY);
    types.Remove(SECURITY_EVENTS);
    types.Remove(SEGMENTATION);
    types.Remove(SEND_TAB_TO_SELF);
    types.Remove(SHARING_MESSAGE);
    types.Remove(USER_CONSENTS);
    types.Remove(USER_EVENTS);
    types.Remove(WORKSPACE_DESK);
  }
  return types;
}

ModelTypeSet SyncUserSettingsImpl::GetEncryptedDataTypes() const {
  return crypto_->GetEncryptedDataTypes();
}

bool SyncUserSettingsImpl::IsEncryptedDatatypeEnabled() const {
  const ModelTypeSet preferred_types = GetPreferredDataTypes();
  const ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.HasAll(AlwaysEncryptedUserTypes()));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

}  // namespace syncer

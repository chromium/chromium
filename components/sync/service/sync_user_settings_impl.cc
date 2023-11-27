// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_user_settings_impl.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/features.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/sync_service_crypto.h"
#include "components/version_info/version_info.h"

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

SyncUserSettingsImpl::SyncUserSettingsImpl(Delegate* delegate,
                                           SyncServiceCrypto* crypto,
                                           SyncPrefs* prefs,
                                           ModelTypeSet registered_model_types)
    : delegate_(delegate),
      crypto_(crypto),
      prefs_(prefs),
      registered_model_types_(registered_model_types) {
  CHECK(delegate_);
  CHECK(crypto_);
  CHECK(prefs_);
}

SyncUserSettingsImpl::~SyncUserSettingsImpl() = default;

bool SyncUserSettingsImpl::IsInitialSyncFeatureSetupComplete() const {
  return prefs_->IsInitialSyncFeatureSetupComplete();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SyncUserSettingsImpl::SetInitialSyncFeatureSetupComplete(
    SyncFirstSetupCompleteSource source) {
  if (IsInitialSyncFeatureSetupComplete()) {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("Signin.SyncFirstSetupCompleteSource", source);
  prefs_->SetInitialSyncFeatureSetupComplete();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

bool SyncUserSettingsImpl::IsSyncEverythingEnabled() const {
  return prefs_->HasKeepEverythingSynced();
}

UserSelectableTypeSet SyncUserSettingsImpl::GetSelectedTypes() const {
  UserSelectableTypeSet types;

  switch (delegate_->GetSyncAccountStateForPrefs()) {
    case SyncPrefs::SyncAccountState::kNotSignedIn: {
      return UserSelectableTypeSet();
    }
    case SyncPrefs::SyncAccountState::kSignedInNotSyncing: {
      signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(
          delegate_->GetSyncAccountInfoForPrefs().gaia);
      types = prefs_->GetSelectedTypesForAccount(gaia_id_hash);
      break;
    }
    case SyncPrefs::SyncAccountState::kSyncing: {
      types = prefs_->GetSelectedTypesForSyncingUser();
      break;
    }
  }
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

bool SyncUserSettingsImpl::IsTypeManagedByCustodian(
    UserSelectableType type) const {
  return prefs_->IsTypeManagedByCustodian(type);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
int SyncUserSettingsImpl::GetNumberOfAccountsWithPasswordsSelected() const {
  return prefs_->GetNumberOfAccountsWithPasswordsSelected();
}
#endif

void SyncUserSettingsImpl::SetSelectedTypes(bool sync_everything,
                                            UserSelectableTypeSet types) {
  UserSelectableTypeSet registered_types = GetRegisteredSelectableTypes();
  DCHECK(registered_types.HasAll(types))
      << "\n registered: " << UserSelectableTypeSetToString(registered_types)
      << "\n setting to: " << UserSelectableTypeSetToString(types);

  switch (delegate_->GetSyncAccountStateForPrefs()) {
    case SyncPrefs::SyncAccountState::kNotSignedIn:
      // TODO(crbug.com/1505100): Convert to NOTREACHED_NORETURN.
      DUMP_WILL_BE_NOTREACHED_NORETURN()
          << "Must not set selected types while signed out";
      break;
    case SyncPrefs::SyncAccountState::kSignedInNotSyncing:
      for (UserSelectableType type : registered_types) {
        SetSelectedType(type, types.Has(type) || sync_everything);
      }
      break;
    case SyncPrefs::SyncAccountState::kSyncing:
      prefs_->SetSelectedTypesForSyncingUser(sync_everything, registered_types,
                                             types);
      break;
  }
}

void SyncUserSettingsImpl::SetSelectedType(UserSelectableType type,
                                           bool is_type_on) {
  UserSelectableTypeSet registered_types = GetRegisteredSelectableTypes();
  CHECK(registered_types.Has(type));

  switch (delegate_->GetSyncAccountStateForPrefs()) {
    case SyncPrefs::SyncAccountState::kNotSignedIn: {
      // TODO(crbug.com/1505100): Convert to NOTREACHED_NORETURN.
      DUMP_WILL_BE_NOTREACHED_NORETURN()
          << "Must not set selected types while signed out";
      break;
    }
    case SyncPrefs::SyncAccountState::kSignedInNotSyncing: {
      signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(
          delegate_->GetSyncAccountInfoForPrefs().gaia);
      prefs_->SetSelectedTypeForAccount(type, is_type_on, gaia_id_hash);
      break;
    }
    case SyncPrefs::SyncAccountState::kSyncing: {
      DUMP_WILL_BE_CHECK(!IsSyncEverythingEnabled());
      syncer::UserSelectableTypeSet selected_types =
          is_type_on ? base::Union(GetSelectedTypes(), {type})
                     : base::Difference(GetSelectedTypes(), {type});
      SetSelectedTypes(IsSyncEverythingEnabled(), selected_types);
      break;
    }
  }
}

void SyncUserSettingsImpl::KeepAccountSettingsPrefsOnlyForUsers(
    const std::vector<signin::GaiaIdHash>& available_gaia_ids) {
  prefs_->KeepAccountSettingsPrefsOnlyForUsers(available_gaia_ids);
}

#if BUILDFLAG(IS_IOS)
void SyncUserSettingsImpl::SetBookmarksAndReadingListAccountStorageOptIn(
    bool value) {
  prefs_->SetBookmarksAndReadingListAccountStorageOptIn(value);
}
#endif  // BUILDFLAG(IS_IOS)

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
void SyncUserSettingsImpl::SetSyncFeatureDisabledViaDashboard() {
  prefs_->SetSyncFeatureDisabledViaDashboard();
}

void SyncUserSettingsImpl::ClearSyncFeatureDisabledViaDashboard() {
  prefs_->ClearSyncFeatureDisabledViaDashboard();
}

bool SyncUserSettingsImpl::IsSyncFeatureDisabledViaDashboard() const {
  return prefs_->IsSyncFeatureDisabledViaDashboard();
}

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
  return delegate_->IsCustomPassphraseAllowed();
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
  // TODO(crbug.com/1466401): Either make this method return a Tribool, so the
  // "unknown" case is properly communicated, or just remove it altogether
  // (callers can always use the global IsExplicitPassphrase() helper).
  absl::optional<PassphraseType> type = GetPassphraseType();
  if (!type.has_value()) {
    return false;
  }
  return IsExplicitPassphrase(*type);
}

base::Time SyncUserSettingsImpl::GetExplicitPassphraseTime() const {
  return crypto_->GetExplicitPassphraseTime();
}

absl::optional<PassphraseType> SyncUserSettingsImpl::GetPassphraseType() const {
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

  // Control types (in practice, NIGORI) are always considered "preferred", even
  // though they're technically not registered.
  types.PutAll(ControlTypes());

  static_assert(47 == GetNumModelTypes(),
                "If adding a new sync data type, update the list below below if"
                " you want to disable the new data type for local sync.");
  if (prefs_->IsLocalSyncEnabled()) {
    types.Remove(APP_LIST);
    types.Remove(AUTOFILL_WALLET_OFFER);
    types.Remove(AUTOFILL_WALLET_USAGE);
    types.Remove(CONTACT_INFO);
    types.Remove(HISTORY);
    types.Remove(INCOMING_PASSWORD_SHARING_INVITATION);
    types.Remove(OUTGOING_PASSWORD_SHARING_INVITATION);
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

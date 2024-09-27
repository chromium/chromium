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
#include "components/signin/public/base/signin_switches.h"
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

// Converts |selected_types| to the corresponding DataTypeSet (e.g.
// {kExtensions} becomes {EXTENSIONS, EXTENSION_SETTINGS}).
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

int GetCurrentMajorProductVersion() {
  DCHECK(version_info::GetVersion().IsValid());
  return version_info::GetVersion().components()[0];
}

// Checks if the AUTOFILL_WALLET_CREDENTIAL should be ignored if it is the only
// encrypted datatype.
bool ShouldAutofillWalletCredentialBeIgnoredIfOnlyEncryptedType(
    const SyncPrefs& prefs) {
  // Explicit sign-in to the browser via native UI, making this scenario an edge
  // case as more features will usually be enabled, including PASSWORDS. Thus,
  // AUTOFILL_WALLET_CREDENTIAL is not the only active encrypted type.
  if (prefs.IsExplicitBrowserSignin()) {
    return false;
  }
  // Similar to above: more features will usually be enabled, including
  // PASSWORDS, making this an edge case.
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return false;
  }
  // AUTOFILL_WALLET_CREDENTIAL is the only active encrypted type for the
  // previously signed-in-non-syncing users.
  return true;
}

}  // namespace

SyncUserSettingsImpl::SyncUserSettingsImpl(Delegate* delegate,
                                           SyncServiceCrypto* crypto,
                                           SyncPrefs* prefs,
                                           DataTypeSet registered_data_types)
    : delegate_(delegate),
      crypto_(crypto),
      prefs_(prefs),
      registered_data_types_(registered_data_types) {
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

SyncUserSettings::UserSelectableTypePrefState
SyncUserSettingsImpl::GetTypePrefStateForAccount(
    UserSelectableType type) const {
  if (delegate_->GetSyncAccountStateForPrefs() !=
      SyncPrefs::SyncAccountState::kSignedInNotSyncing) {
    return SyncUserSettings::UserSelectableTypePrefState::kNotApplicable;
  }
  signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(
      delegate_->GetSyncAccountInfoForPrefs().gaia);
  if (prefs_->IsTypeDisabledByUserForAccount(type, gaia_id_hash)) {
    return SyncUserSettings::UserSelectableTypePrefState::kDisabled;
  }
  return SyncUserSettings::UserSelectableTypePrefState::kEnabledOrDefault;
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
      // TODO(crbug.com/40945692): Convert to NOTREACHED.
      DUMP_WILL_BE_NOTREACHED()
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
      // TODO(crbug.com/40945692): Convert to NOTREACHED.
      DUMP_WILL_BE_NOTREACHED()
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

UserSelectableTypeSet SyncUserSettingsImpl::GetRegisteredSelectableTypes()
    const {
  UserSelectableTypeSet registered_types;
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    if (!base::Intersection(registered_data_types_,
                            UserSelectableTypeToAllDataTypes(type))
             .empty()) {
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
    if (!base::Intersection(registered_data_types_,
                            UserSelectableOsTypeToAllDataTypes(type))
             .empty()) {
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
  return IsEncryptedDatatypePreferred() && IsPassphraseRequired();
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
  return IsEncryptedDatatypePreferred() && crypto_->IsTrustedVaultKeyRequired();
}

bool SyncUserSettingsImpl::IsTrustedVaultRecoverabilityDegraded() const {
  return IsEncryptedDatatypePreferred() &&
         crypto_->IsTrustedVaultRecoverabilityDegraded();
}

bool SyncUserSettingsImpl::IsUsingExplicitPassphrase() const {
  // TODO(crbug.com/40923935): Either make this method return a Tribool, so the
  // "unknown" case is properly communicated, or just remove it altogether
  // (callers can always use the global IsExplicitPassphrase() helper).
  std::optional<PassphraseType> type = GetPassphraseType();
  if (!type.has_value()) {
    return false;
  }
  return IsExplicitPassphrase(*type);
}

base::Time SyncUserSettingsImpl::GetExplicitPassphraseTime() const {
  return crypto_->GetExplicitPassphraseTime();
}

std::optional<PassphraseType> SyncUserSettingsImpl::GetPassphraseType() const {
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

void SyncUserSettingsImpl::SetExplicitPassphraseDecryptionNigoriKey(
    std::unique_ptr<Nigori> nigori) {
  return crypto_->SetExplicitPassphraseDecryptionNigoriKey(std::move(nigori));
}

std::unique_ptr<Nigori>
SyncUserSettingsImpl::GetExplicitPassphraseDecryptionNigoriKey() const {
  return crypto_->GetExplicitPassphraseDecryptionNigoriKey();
}

DataTypeSet SyncUserSettingsImpl::GetPreferredDataTypes() const {
  DataTypeSet types = UserSelectableTypesToDataTypes(GetSelectedTypes());
  types.PutAll(AlwaysPreferredUserTypes());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  types.PutAll(UserSelectableOsTypesToDataTypes(GetSelectedOsTypes()));
#endif
  types.RetainAll(registered_data_types_);

  // Control types (in practice, NIGORI) are always considered "preferred", even
  // though they're technically not registered.
  types.PutAll(ControlTypes());

  static_assert(53 == GetNumDataTypes(),
                "If adding a new sync data type, update the list below below if"
                " you want to disable the new data type for local sync, aka"
                " roaming profiles on Windows.");
  if (prefs_->IsLocalSyncEnabled()) {
    types.Remove(APP_LIST);
    // Note: AUTOFILL_WALLET_CREDENTIAL *is* supported - the user can still save
    // CVVs for local credit cards.
    types.Remove(AUTOFILL_WALLET_DATA);
    types.Remove(AUTOFILL_WALLET_METADATA);
    types.Remove(AUTOFILL_WALLET_OFFER);
    types.Remove(AUTOFILL_WALLET_USAGE);
    types.Remove(COLLABORATION_GROUP);
    types.Remove(CONTACT_INFO);
    types.Remove(COOKIES);
    types.Remove(HISTORY);
    types.Remove(HISTORY_DELETE_DIRECTIVES);
    types.Remove(INCOMING_PASSWORD_SHARING_INVITATION);
    types.Remove(OUTGOING_PASSWORD_SHARING_INVITATION);
    types.Remove(PLUS_ADDRESS);
    types.Remove(PLUS_ADDRESS_SETTING);
    types.Remove(SECURITY_EVENTS);
    types.Remove(SEND_TAB_TO_SELF);
    types.Remove(SHARED_TAB_GROUP_DATA);
    types.Remove(SHARING_MESSAGE);
    types.Remove(USER_CONSENTS);
    types.Remove(USER_EVENTS);
    types.Remove(WORKSPACE_DESK);
  }
  return types;
}

DataTypeSet SyncUserSettingsImpl::GetAllEncryptedDataTypes() const {
  return crypto_->GetAllEncryptedDataTypes();
}

bool SyncUserSettingsImpl::IsEncryptedDatatypePreferred() const {
  DataTypeSet preferred_types = GetPreferredDataTypes();
  const DataTypeSet encrypted_types = GetAllEncryptedDataTypes();
  DCHECK(encrypted_types.HasAll(AlwaysEncryptedUserTypes()));
  if (ShouldAutofillWalletCredentialBeIgnoredIfOnlyEncryptedType(*prefs_)) {
    // Remove AUTOFILL_WALLET_CREDENTIAL from the set to avoid that the
    // function returns true for the case where the set ONLY includes
    // AUTOFILL_WALLET_CREDENTIAL. This feature alone is not sufficient to
    // trigger error UI, which may be confusing to some users given that strings
    // may allude to passwords. This is a side effect of
    // AUTOFILL_WALLET_CREDENTIAL being listed as AlwaysEncryptedUserTypes().
    preferred_types.Remove(syncer::AUTOFILL_WALLET_CREDENTIAL);
  }
  return !Intersection(preferred_types, encrypted_types).empty();
}

std::string SyncUserSettingsImpl::GetEncryptionBootstrapToken() const {
  const std::string& gaia_id = delegate_->GetSyncAccountInfoForPrefs().gaia;
  if (gaia_id.empty()) {
    return std::string();
  }
  signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(gaia_id);
  CHECK(gaia_id_hash.IsValid());
  return prefs_->GetEncryptionBootstrapTokenForAccount(gaia_id_hash);
}

void SyncUserSettingsImpl::SetEncryptionBootstrapToken(
    const std::string& token) {
  const std::string& gaia_id = delegate_->GetSyncAccountInfoForPrefs().gaia;
  if (gaia_id.empty()) {
    // TODO(crbug.com/40945692): Convert to NOTREACHED.
    DUMP_WILL_BE_NOTREACHED() << "Must not set passphrase while signed out";
    return;
  }
  signin::GaiaIdHash gaia_id_hash = signin::GaiaIdHash::FromGaiaId(gaia_id);
  CHECK(gaia_id_hash.IsValid());
  prefs_->SetEncryptionBootstrapTokenForAccount(token, gaia_id_hash);
}

}  // namespace syncer

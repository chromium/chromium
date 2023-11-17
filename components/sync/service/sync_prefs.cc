// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_prefs.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_feature_status_for_migrations_recorder.h"

namespace syncer {

namespace {

// Historic artifact for payment methods, migrated since 2023/07 to
// prefs::internal::kSyncPayments to make the name consistent with other
// user-selectable types.
constexpr char kObsoleteAutofillWalletImportEnabled[] =
    "autofill.wallet_import_enabled";

// Boolean representing whether kObsoleteAutofillWalletImportEnabled was
// migrated to the new pref representing a UserSelectableType. Should be cleaned
// up together with the migration code (after 2024-07).
constexpr char kObsoleteAutofillWalletImportEnabledMigrated[] =
    "sync.autofill_wallet_import_enabled_migrated";

// State of the migration done by
// MaybeMigratePrefsForSyncToSigninPart1() and
// MaybeMigratePrefsForSyncToSigninPart2(). Should be cleaned up
// after those migration methods are gone.
constexpr char kSyncToSigninMigrationState[] =
    "sync.sync_to_signin_migration_state";

constexpr int kNotMigrated = 0;
constexpr int kMigratedPart1ButNot2 = 1;
constexpr int kMigratedPart2AndFullyDone = 2;

}  // namespace

SyncPrefObserver::~SyncPrefObserver() = default;

SyncPrefs::SyncPrefs(PrefService* pref_service) : pref_service_(pref_service) {
  DCHECK(pref_service);
  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(
      prefs::internal::kSyncManaged, pref_service_,
      base::BindRepeating(&SyncPrefs::OnSyncManagedPrefChanged,
                          base::Unretained(this)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  pref_initial_sync_feature_setup_complete_.Init(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, pref_service_,
      base::BindRepeating(&SyncPrefs::OnFirstSetupCompletePrefChange,
                          base::Unretained(this)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Cache the value of the kEnableLocalSyncBackend pref to avoid it flipping
  // during the lifetime of the service.
  local_sync_enabled_ =
      pref_service_->GetBoolean(prefs::kEnableLocalSyncBackend);
}

SyncPrefs::~SyncPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void SyncPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Actual user-controlled preferences.
  registry->RegisterBooleanPref(prefs::internal::kSyncKeepEverythingSynced,
                                true);
#if BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(
      prefs::internal::kBookmarksAndReadingListAccountStorageOptIn, false);
#endif  // BUILDFLAG(IS_IOS)
  registry->RegisterDictionaryPref(prefs::internal::kSelectedTypesPerAccount);
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    RegisterTypeSelectedPref(registry, type);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(prefs::internal::kSyncDisabledViaDashboard,
                                false);
  registry->RegisterBooleanPref(prefs::internal::kSyncAllOsTypes, true);
  registry->RegisterBooleanPref(prefs::internal::kSyncOsApps, false);
  registry->RegisterBooleanPref(prefs::internal::kSyncOsPreferences, false);
  registry->RegisterBooleanPref(prefs::internal::kSyncWifiConfigurations,
                                false);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  registry->RegisterBooleanPref(prefs::internal::kSyncAppsEnabledByOs, false);
#endif

  registry->RegisterBooleanPref(kObsoleteAutofillWalletImportEnabledMigrated,
                                false);
  registry->RegisterIntegerPref(kSyncToSigninMigrationState, kNotMigrated);

  // The passphrase type, determined upon the first engine initialization.
  registry->RegisterIntegerPref(
      prefs::internal::kSyncCachedPassphraseType,
      sync_pb::NigoriSpecifics_PassphraseType_UNKNOWN);
  // The encryption bootstrap token represents a user-entered passphrase.
  registry->RegisterStringPref(prefs::internal::kSyncEncryptionBootstrapToken,
                               std::string());

  registry->RegisterBooleanPref(prefs::internal::kSyncManaged, false);
  registry->RegisterIntegerPref(
      prefs::internal::kSyncPassphrasePromptMutedProductVersion, 0);
  registry->RegisterBooleanPref(prefs::kEnableLocalSyncBackend, false);
  registry->RegisterFilePathPref(prefs::kLocalSyncBackendDir, base::FilePath());

  SyncFeatureStatusForMigrationsRecorder::RegisterProfilePrefs(registry);

  // Obsolete prefs (registered for migrations only).
  registry->RegisterBooleanPref(kObsoleteAutofillWalletImportEnabled, true);
}

void SyncPrefs::AddObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.AddObserver(sync_pref_observer);
}

void SyncPrefs::RemoveObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.RemoveObserver(sync_pref_observer);
}

bool SyncPrefs::IsInitialSyncFeatureSetupComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  return pref_service_->GetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SyncPrefs::SetInitialSyncFeatureSetupComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, true);
}

void SyncPrefs::ClearInitialSyncFeatureSetupComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncKeepEverythingSynced);
}

UserSelectableTypeSet SyncPrefs::GetSelectedTypesForAccount(
    const signin::GaiaIdHash& gaia_id_hash) const {
  CHECK(base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos))
      << "Call GetSelectedTypes instead";

  UserSelectableTypeSet selected_types;

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    if (!IsTypeSupportedInTransportMode(type)) {
      continue;
    }
    const char* pref_name = GetPrefNameForType(type);
    DCHECK(pref_name);
    bool type_enabled = false;
    if (IsTypeManagedByPolicy(type) || IsTypeManagedByCustodian(type)) {
      type_enabled = pref_service_->GetBoolean(pref_name);
    } else {
      const base::Value::Dict* account_settings =
          pref_service_->GetDict(prefs::internal::kSelectedTypesPerAccount)
              .FindDict(gaia_id_hash.ToBase64());
      absl::optional<bool> pref_value;
      if (account_settings) {
        pref_value = account_settings->FindBool(pref_name);
      }
      if (pref_value.has_value()) {
        type_enabled = *pref_value;
      } else {
        // All types except for History and Tabs are enabled by default.
        type_enabled = type != UserSelectableType::kHistory &&
                       type != UserSelectableType::kTabs;
      }
    }
    if (type_enabled) {
      selected_types.Put(type);
    }
  }

  return selected_types;
}

UserSelectableTypeSet SyncPrefs::GetSelectedTypes(
    SyncAccountState account_state) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UserSelectableTypeSet selected_types;

  switch (account_state) {
    case SyncAccountState::kNotSignedIn: {
      break;
    }
    case SyncAccountState::kSignedInNotSyncing: {
      for (UserSelectableType type : UserSelectableTypeSet::All()) {
        if (!IsTypeSupportedInTransportMode(type)) {
          continue;
        }
        const char* pref_name = GetPrefNameForType(type);
        DCHECK(pref_name);
        CHECK(!base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos))
            << "Call GetSelectedTypesForAccount instead";

        // TODO(crbug.com/1455963): Find a better solution than manually
        // overriding the prefs' default values.
        if (pref_service_->GetBoolean(pref_name) ||
            pref_service_->FindPreference(pref_name)->IsDefaultValue()) {
          // In transport-mode, individual types are considered enabled by
          // default.
#if BUILDFLAG(IS_IOS)
          // In transport-only mode, bookmarks and reading list require an
          // additional opt-in.
          // TODO(crbug.com/1440628): Cleanup the temporary behaviour of an
          // additional opt in for Bookmarks and Reading Lists.
          if ((type == UserSelectableType::kBookmarks ||
               type == UserSelectableType::kReadingList) &&
              !pref_service_->GetBoolean(
                  prefs::internal::
                      kBookmarksAndReadingListAccountStorageOptIn)) {
            continue;
          }
#endif  // BUILDFLAG(IS_IOS)
          selected_types.Put(type);
        }
      }
      break;
    }
    case SyncAccountState::kSyncing: {
      for (UserSelectableType type : UserSelectableTypeSet::All()) {
        const char* pref_name = GetPrefNameForType(type);
        DCHECK(pref_name);
        if (pref_service_->GetBoolean(pref_name) ||
            (!IsTypeManagedByPolicy(type) && !IsTypeManagedByCustodian(type) &&
             pref_service_->GetBoolean(
                 prefs::internal::kSyncKeepEverythingSynced))) {
          // In full-sync mode, the "sync everything" bit is honored. If it's
          // true, all types are considered selected, irrespective of their
          // individual prefs.
          selected_types.Put(type);
        }
      }
      break;
    }
  }

  return selected_types;
}

bool SyncPrefs::IsTypeManagedByPolicy(UserSelectableType type) const {
  const char* pref_name = GetPrefNameForType(type);
  CHECK(pref_name);
  return pref_service_->IsManagedPreference(pref_name);
}

bool SyncPrefs::IsTypeManagedByCustodian(UserSelectableType type) const {
  const char* pref_name = GetPrefNameForType(type);
  CHECK(pref_name);
  return pref_service_->IsPreferenceManagedByCustodian(pref_name);
}

void SyncPrefs::SetSelectedTypes(bool keep_everything_synced,
                                 UserSelectableTypeSet registered_types,
                                 UserSelectableTypeSet selected_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pref_service_->SetBoolean(prefs::internal::kSyncKeepEverythingSynced,
                            keep_everything_synced);

  for (UserSelectableType type : registered_types) {
    const char* pref_name = GetPrefNameForType(type);
    pref_service_->SetBoolean(pref_name, selected_types.Has(type));
  }

  // Payments integration might have changed, so report as true.
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange(
        /*payments_integration_enabled_changed=*/true);
  }
}

void SyncPrefs::SetSelectedTypeForAccount(
    UserSelectableType type,
    bool is_type_on,
    const signin::GaiaIdHash& gaia_id_hash) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  {
    ScopedDictPrefUpdate update_selected_types_dict(
        pref_service_, prefs::internal::kSelectedTypesPerAccount);
    base::Value::Dict* account_settings =
        update_selected_types_dict->EnsureDict(gaia_id_hash.ToBase64());
    account_settings->Set(GetPrefNameForType(type), is_type_on);
  }

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange(
        /*payments_integration_enabled_changed=*/
        type == UserSelectableType::kPayments);
  }
}

void SyncPrefs::KeepAccountSettingsPrefsOnlyForUsers(
    const std::vector<signin::GaiaIdHash>& available_gaia_ids) {
  std::vector<std::string> removed_identities;
  for (std::pair<const std::string&, const base::Value&> account_settings :
       pref_service_->GetDict(prefs::internal::kSelectedTypesPerAccount)) {
    if (!base::Contains(available_gaia_ids, signin::GaiaIdHash::FromBase64(
                                                account_settings.first))) {
      removed_identities.push_back(account_settings.first);
    }
  }
  if (!removed_identities.empty()) {
    {
      ScopedDictPrefUpdate update_selected_types_dict(
          pref_service_, prefs::internal::kSelectedTypesPerAccount);
      base::Value::Dict& all_accounts = update_selected_types_dict.Get();
      for (const auto& account_id : removed_identities) {
        all_accounts.Remove(account_id);
      }
    }
  }
}

#if BUILDFLAG(IS_IOS)
void SyncPrefs::SetBookmarksAndReadingListAccountStorageOptIn(bool value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(
      prefs::internal::kBookmarksAndReadingListAccountStorageOptIn, value);

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange(
        /*payments_integration_enabled_changed=*/false);
  }
}

bool SyncPrefs::IsOptedInForBookmarksAndReadingListAccountStorageForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(
      prefs::internal::kBookmarksAndReadingListAccountStorageOptIn);
}

void SyncPrefs::ClearBookmarksAndReadingListAccountStorageOptIn() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(
      prefs::internal::kBookmarksAndReadingListAccountStorageOptIn);
}
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool SyncPrefs::IsSyncFeatureDisabledViaDashboard() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncDisabledViaDashboard);
}

void SyncPrefs::SetSyncFeatureDisabledViaDashboard() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::internal::kSyncDisabledViaDashboard, true);
}

void SyncPrefs::ClearSyncFeatureDisabledViaDashboard() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::internal::kSyncDisabledViaDashboard);
}

bool SyncPrefs::IsSyncAllOsTypesEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncAllOsTypes);
}

UserSelectableOsTypeSet SyncPrefs::GetSelectedOsTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UserSelectableOsTypeSet selected_types;
  const bool sync_all_os_types = IsSyncAllOsTypesEnabled();
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    const char* pref_name = GetPrefNameForOsType(type);
    DCHECK(pref_name);
    // If the type is managed, |sync_all_os_types| is ignored for this type.
    if (pref_service_->GetBoolean(pref_name) ||
        (sync_all_os_types && !IsOsTypeManagedByPolicy(type))) {
      selected_types.Put(type);
    }
  }
  return selected_types;
}

bool SyncPrefs::IsOsTypeManagedByPolicy(UserSelectableOsType type) const {
  const char* pref_name = GetPrefNameForOsType(type);
  CHECK(pref_name);
  return pref_service_->IsManagedPreference(pref_name);
}

void SyncPrefs::SetSelectedOsTypes(bool sync_all_os_types,
                                   UserSelectableOsTypeSet registered_types,
                                   UserSelectableOsTypeSet selected_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::internal::kSyncAllOsTypes,
                            sync_all_os_types);
  for (UserSelectableOsType type : registered_types) {
    const char* pref_name = GetPrefNameForOsType(type);
    DCHECK(pref_name);
    pref_service_->SetBoolean(pref_name, selected_types.Has(type));
  }
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange(
        /*payments_integration_enabled_changed=*/false);
  }
}

// static
const char* SyncPrefs::GetPrefNameForOsTypeForTesting(
    UserSelectableOsType type) {
  return GetPrefNameForOsType(type);
}

// static
const char* SyncPrefs::GetPrefNameForOsType(UserSelectableOsType type) {
  switch (type) {
    case UserSelectableOsType::kOsApps:
      return prefs::internal::kSyncOsApps;
    case UserSelectableOsType::kOsPreferences:
      return prefs::internal::kSyncOsPreferences;
    case UserSelectableOsType::kOsWifiConfigurations:
      return prefs::internal::kSyncWifiConfigurations;
  }
  NOTREACHED();
  return nullptr;
}

// static
void SyncPrefs::SetOsTypeDisabledByPolicy(PrefValueMap* policy_prefs,
                                          UserSelectableOsType type) {
  const char* pref_name = syncer::SyncPrefs::GetPrefNameForOsType(type);
  CHECK(pref_name);
  policy_prefs->SetValue(pref_name, base::Value(false));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool SyncPrefs::IsAppsSyncEnabledByOs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncAppsEnabledByOs);
}

void SyncPrefs::SetAppsSyncEnabledByOs(bool apps_sync_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::internal::kSyncAppsEnabledByOs,
                            apps_sync_enabled);
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange(
        /*payments_integration_enabled_changed=*/false);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

bool SyncPrefs::IsSyncClientDisabledByPolicy() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncManaged);
}

absl::optional<PassphraseType> SyncPrefs::GetCachedPassphraseType() const {
  return ProtoPassphraseInt32ToEnum(
      pref_service_->GetInteger(prefs::internal::kSyncCachedPassphraseType));
}

void SyncPrefs::SetCachedPassphraseType(PassphraseType passphrase_type) {
  pref_service_->SetInteger(prefs::internal::kSyncCachedPassphraseType,
                            EnumPassphraseTypeToProto(passphrase_type));
}

void SyncPrefs::ClearCachedPassphraseType() {
  pref_service_->ClearPref(prefs::internal::kSyncCachedPassphraseType);
}

std::string SyncPrefs::GetEncryptionBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(
      prefs::internal::kSyncEncryptionBootstrapToken);
}

void SyncPrefs::SetEncryptionBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::internal::kSyncEncryptionBootstrapToken,
                           token);
}

void SyncPrefs::ClearEncryptionBootstrapToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::internal::kSyncEncryptionBootstrapToken);
}

// static
const char* SyncPrefs::GetPrefNameForTypeForTesting(UserSelectableType type) {
  return GetPrefNameForType(type);
}

// static
const char* SyncPrefs::GetPrefNameForType(UserSelectableType type) {
  switch (type) {
    case UserSelectableType::kBookmarks:
      return prefs::internal::kSyncBookmarks;
    case UserSelectableType::kPreferences:
      return prefs::internal::kSyncPreferences;
    case UserSelectableType::kPasswords:
      return prefs::internal::kSyncPasswords;
    case UserSelectableType::kAutofill:
      return prefs::internal::kSyncAutofill;
    case UserSelectableType::kThemes:
      return prefs::internal::kSyncThemes;
    case UserSelectableType::kHistory:
      return prefs::internal::kSyncHistory;
    case UserSelectableType::kExtensions:
      return prefs::internal::kSyncExtensions;
    case UserSelectableType::kApps:
      return prefs::internal::kSyncApps;
    case UserSelectableType::kReadingList:
      return prefs::internal::kSyncReadingList;
    case UserSelectableType::kTabs:
      return prefs::internal::kSyncTabs;
    case UserSelectableType::kSavedTabGroups:
      return prefs::internal::kSyncSavedTabGroups;
    case UserSelectableType::kPayments:
      return prefs::internal::kSyncPayments;
  }
  NOTREACHED();
  return nullptr;
}

// static
void SyncPrefs::SetTypeDisabledByPolicy(PrefValueMap* policy_prefs,
                                        UserSelectableType type) {
  const char* pref_name = syncer::SyncPrefs::GetPrefNameForType(type);
  CHECK(pref_name);
  CHECK(policy_prefs);
  policy_prefs->SetValue(pref_name, base::Value(false));
}

// static void
void SyncPrefs::SetTypeDisabledByCustodian(PrefValueMap* supervised_user_prefs,
                                           UserSelectableType type) {
  const char* pref_name = syncer::SyncPrefs::GetPrefNameForType(type);
  CHECK(pref_name);
  CHECK(supervised_user_prefs);
  supervised_user_prefs->SetValue(pref_name, base::Value(false));
}

// static
bool SyncPrefs::IsTypeSupportedInTransportMode(UserSelectableType type) {
  // Not all types are supported in transport mode, and many require one or more
  // Features to be enabled.
  switch (type) {
    case UserSelectableType::kBookmarks:
      return base::FeatureList::IsEnabled(kEnableBookmarksAccountStorage);
    case UserSelectableType::kReadingList:
      return base::FeatureList::IsEnabled(
                 kReadingListEnableDualReadingListModel) &&
             base::FeatureList::IsEnabled(
                 kReadingListEnableSyncTransportModeUponSignIn);
    case UserSelectableType::kPreferences:
      return base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos) &&
             base::FeatureList::IsEnabled(kEnablePreferencesAccountStorage);
    case UserSelectableType::kPasswords:
      return base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage);
    case UserSelectableType::kAutofill:
      // Note that this logic may lead to kPayments being treated as supported
      // (or even selected) while kAutofill isn't. This goes against the general
      // practice that kPayments depends on kAutofill (when it comes to user
      // choice).
      // TODO(crbug.com/1435431): Update comment once the decoupling is removed.
      return base::FeatureList::IsEnabled(
          kSyncEnableContactInfoDataTypeInTransportMode);
    case UserSelectableType::kPayments:
      // Always supported, since AUTOFILL_WALLET_DATA is supported in
      // transport mode everywhere.
      return true;
    case UserSelectableType::kHistory:
    case UserSelectableType::kTabs:
      return base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos);
    case UserSelectableType::kApps:
    case UserSelectableType::kExtensions:
    case UserSelectableType::kThemes:
    case UserSelectableType::kSavedTabGroups:
      // These types are not supported in transport mode yet.
      return false;
  }
}

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnSyncManagedPrefChange(*pref_sync_managed_);
  }
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SyncPrefs::OnFirstSetupCompletePrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnFirstSetupCompletePrefChange(
        *pref_initial_sync_feature_setup_complete_);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// static
void SyncPrefs::RegisterTypeSelectedPref(PrefRegistrySimple* registry,
                                         UserSelectableType type) {
  const char* pref_name = GetPrefNameForType(type);
  DCHECK(pref_name);
  registry->RegisterBooleanPref(pref_name, false);
}

bool SyncPrefs::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

int SyncPrefs::GetPassphrasePromptMutedProductVersion() const {
  return pref_service_->GetInteger(
      prefs::internal::kSyncPassphrasePromptMutedProductVersion);
}

void SyncPrefs::SetPassphrasePromptMutedProductVersion(int major_version) {
  pref_service_->SetInteger(
      prefs::internal::kSyncPassphrasePromptMutedProductVersion, major_version);
}

void SyncPrefs::ClearPassphrasePromptMutedProductVersion() {
  pref_service_->ClearPref(
      prefs::internal::kSyncPassphrasePromptMutedProductVersion);
}

bool SyncPrefs::MaybeMigratePrefsForSyncToSigninPart1(
    SyncAccountState account_state,
    signin::GaiaIdHash gaia_id_hash) {
  if (!base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos)) {
    // Ensure that the migration runs again when the feature gets enabled.
    pref_service_->ClearPref(kSyncToSigninMigrationState);
    return false;
  }

  // Don't migrate again if this profile was previously migrated.
  if (pref_service_->GetInteger(kSyncToSigninMigrationState) != kNotMigrated) {
    return false;
  }

  if (IsLocalSyncEnabled()) {
    // Special case for local sync: There isn't necessarily a signed-in user
    // (even if the SyncAccountState is kSyncing), so just mark the migration as
    // done.
    pref_service_->SetInteger(kSyncToSigninMigrationState,
                              kMigratedPart2AndFullyDone);
    return false;
  }

  switch (account_state) {
    case SyncAccountState::kNotSignedIn:
    case SyncAccountState::kSyncing: {
      // Nothing to migrate for signed-out or syncing users. Also make sure the
      // second part of the migration does *not* run if a signed-out user does
      // later sign in / turn on sync.
      pref_service_->SetInteger(kSyncToSigninMigrationState,
                                kMigratedPart2AndFullyDone);
      return false;
    }
    case SyncAccountState::kSignedInNotSyncing: {
      pref_service_->SetInteger(kSyncToSigninMigrationState,
                                kMigratedPart1ButNot2);
      // For pre-existing signed-in users, some state needs to be migrated from
      // the global to the account-scoped settings.
      CHECK(gaia_id_hash.IsValid());
      ScopedDictPrefUpdate update_selected_types_dict(
          pref_service_, prefs::internal::kSelectedTypesPerAccount);
      base::Value::Dict* account_settings =
          update_selected_types_dict->EnsureDict(gaia_id_hash.ToBase64());

      // Mostly, the values of the "global" data type prefs get copied to the
      // account-specific ones. But some data types get special treatment.
      for (UserSelectableType type : UserSelectableTypeSet::All()) {
        const char* pref_name = GetPrefNameForType(type);
        CHECK(pref_name);

        // Initial default value: From the global datatype pref (compare to
        // GetSelectedTypes()).
        // TODO(crbug.com/1455963): Find a better solution than manually
        // overriding the prefs' default values.
        bool enabled =
            pref_service_->GetBoolean(pref_name) ||
            pref_service_->FindPreference(pref_name)->IsDefaultValue();

        // History and open tabs do *not* get migrated; they always start out
        // "off".
        if (type == UserSelectableType::kHistory ||
            type == UserSelectableType::kTabs) {
          enabled = false;
        }

        // Settings aka preferences always starts out "off".
        if (type == UserSelectableType::kPreferences) {
          enabled = false;
        }

#if BUILDFLAG(IS_IOS)
        // Bookmarks and reading list remain enabled only if the user previously
        // explicitly opted in.
        if ((type == UserSelectableType::kBookmarks ||
             type == UserSelectableType::kReadingList) &&
            !pref_service_->GetBoolean(
                prefs::internal::kBookmarksAndReadingListAccountStorageOptIn)) {
          enabled = false;
        }
#endif  // BUILDFLAG(IS_IOS)

        account_settings->Set(pref_name, enabled);
      }

      return true;
    }
  }
}

bool SyncPrefs::MaybeMigratePrefsForSyncToSigninPart2(
    signin::GaiaIdHash gaia_id_hash,
    bool is_using_explicit_passphrase) {
  // The migration pref shouldn't be set if the feature is disabled, but if it
  // somehow happened, do *not* run the migration, and clear the pref so that
  // the migration will get triggered again once the feature gets enabled again.
  if (!base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos)) {
    pref_service_->ClearPref(kSyncToSigninMigrationState);
    return false;
  }

  // Only run part 2 of the migration if part 1 has run but part 2 hasn't yet.
  // This ensures that it only runs once.
  if (pref_service_->GetInteger(kSyncToSigninMigrationState) !=
      kMigratedPart1ButNot2) {
    return false;
  }
  pref_service_->SetInteger(kSyncToSigninMigrationState,
                            kMigratedPart2AndFullyDone);

  // The actual migration: For explicit-passphrase users, addresses sync gets
  // disabled by default.
  if (is_using_explicit_passphrase && !IsLocalSyncEnabled()) {
    CHECK(gaia_id_hash.IsValid());
    ScopedDictPrefUpdate update_selected_types_dict(
        pref_service_, prefs::internal::kSelectedTypesPerAccount);
    base::Value::Dict* account_settings =
        update_selected_types_dict->EnsureDict(gaia_id_hash.ToBase64());
    account_settings->Set(GetPrefNameForType(UserSelectableType::kAutofill),
                          false);
    if (!base::FeatureList::IsEnabled(
            syncer::kSyncDecoupleAddressPaymentSettings)) {
      // When the auto fill data type is updated, the payments should be updated
      // too. Payments should not be enabled when auto fill data type disabled.
      // TODO(crbug.com/1435431): This can be removed once kPayments is
      // decoupled from kAutofill.
      account_settings->Set(GetPrefNameForType(UserSelectableType::kPayments),
                            false);
    }
    return true;
  }
  return false;
}

// static
void SyncPrefs::MigrateAutofillWalletImportEnabledPref(
    PrefService* pref_service) {
  if (pref_service->GetBoolean(kObsoleteAutofillWalletImportEnabledMigrated)) {
    // Migration already happened; nothing else needed.
    return;
  }

  const base::Value* autofill_wallet_import_enabled =
      pref_service->GetUserPrefValue(kObsoleteAutofillWalletImportEnabled);

  if (autofill_wallet_import_enabled != nullptr) {
    // If the previous pref was populated explicitly, carry over the value.
    pref_service->SetBoolean(prefs::internal::kSyncPayments,
                             autofill_wallet_import_enabled->GetBool());
  } else if (pref_service->GetBoolean(
                 prefs::internal::kSyncKeepEverythingSynced)) {
    // The old pref isn't explicitly set (defaults to true) and sync-everything
    // is on: in this case there is no need to explicitly set individual
    // UserSelectableType to true.
  } else {
    // There is a special case for very old profiles, created before 2019 (i.e.
    // before https://codereview.chromium.org/2068653003 and similar code
    // changes). In older versions of the UI, it was possible to set
    // kSyncKeepEverythingSynced to false, without populating
    // kObsoleteAutofillWalletImportEnabled. The latter defaults to true, but
    // that's not the case for the new replacement, i.e.
    // prefs::internal::kSyncPayments, so it needs to be populated manually to
    // migrate the old behavior.
    pref_service->SetBoolean(prefs::internal::kSyncPayments, true);
  }

  pref_service->ClearPref(kObsoleteAutofillWalletImportEnabled);
  pref_service->SetBoolean(kObsoleteAutofillWalletImportEnabledMigrated, true);
}

void SyncPrefs::MarkPartialSyncToSigninMigrationFullyDone() {
  // If the first part of the migration has run, but the second part has not,
  // then mark the migration as fully done - at this point (after signout)
  // there's no more need for any migration.
  // In all other cases (migration never even started, or completed fully),
  // nothing to be done here.
  if (pref_service_->GetInteger(kSyncToSigninMigrationState) ==
      kMigratedPart1ButNot2) {
    pref_service_->SetInteger(kSyncToSigninMigrationState,
                              kMigratedPart2AndFullyDone);
  }
}

}  // namespace syncer

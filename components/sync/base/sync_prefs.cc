// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"

namespace {

// Whether MaybeMigratePrefsForReplacingSyncWithSignin() has run in this
// profile. Should be cleaned up after
// MaybeMigratePrefsForReplacingSyncWithSignin() itself is gone.
constexpr char kReplacingSyncWithSigninMigrated[] =
    "sync.replacing_sync_with_signin_migrated";

}  // namespace

namespace syncer {

SyncPrefObserver::~SyncPrefObserver() = default;

SyncPrefs::SyncPrefs(PrefService* pref_service) : pref_service_(pref_service) {
  DCHECK(pref_service);
  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(
      prefs::internal::kSyncManaged, pref_service_,
      base::BindRepeating(&SyncPrefs::OnSyncManagedPrefChanged,
                          base::Unretained(this)));
  pref_initial_sync_feature_setup_complete_.Init(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, pref_service_,
      base::BindRepeating(&SyncPrefs::OnFirstSetupCompletePrefChange,
                          base::Unretained(this)));

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
  registry->RegisterBooleanPref(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete, false);
  registry->RegisterBooleanPref(prefs::internal::kSyncRequested, false);
  registry->RegisterBooleanPref(prefs::internal::kSyncKeepEverythingSynced,
                                true);
#if BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(
      prefs::internal::kBookmarksAndReadingListAccountStorageOptIn, false);
#endif  // BUILDFLAG(IS_IOS)
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    RegisterTypeSelectedPref(registry, type);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(prefs::internal::kSyncAllOsTypes, true);
  registry->RegisterBooleanPref(prefs::internal::kSyncOsApps, false);
  registry->RegisterBooleanPref(prefs::internal::kSyncOsPreferences, false);
  registry->RegisterBooleanPref(prefs::internal::kSyncWifiConfigurations,
                                false);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  registry->RegisterBooleanPref(prefs::internal::kSyncAppsEnabledByOs, false);
#endif

  registry->RegisterBooleanPref(kReplacingSyncWithSigninMigrated, false);

  // The encryption bootstrap token represents a user-entered passphrase.
  registry->RegisterStringPref(prefs::internal::kSyncEncryptionBootstrapToken,
                               std::string());

  registry->RegisterBooleanPref(prefs::internal::kSyncManaged, false);
  registry->RegisterIntegerPref(
      prefs::internal::kSyncPassphrasePromptMutedProductVersion, 0);
  registry->RegisterBooleanPref(prefs::kEnableLocalSyncBackend, false);
  registry->RegisterFilePathPref(prefs::kLocalSyncBackendDir, base::FilePath());
}

void SyncPrefs::AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.AddObserver(sync_pref_observer);
}

void SyncPrefs::RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.RemoveObserver(sync_pref_observer);
}

bool SyncPrefs::IsInitialSyncFeatureSetupComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(
      prefs::internal::kSyncInitialSyncFeatureSetupComplete);
}

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

bool SyncPrefs::IsSyncRequested() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncRequested);
}

void SyncPrefs::SetSyncRequested(bool is_requested) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::internal::kSyncRequested, is_requested);
}

bool SyncPrefs::IsSyncRequestedSetExplicitly() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // GetUserPrefValue() returns nullptr if there is no user-set value for this
  // pref (there might still be a non-default value, e.g. from a policy, but we
  // explicitly don't care about that here).
  return pref_service_->GetUserPrefValue(prefs::internal::kSyncRequested) !=
         nullptr;
}

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncKeepEverythingSynced);
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
        const char* pref_name = GetPrefNameForType(type);
        DCHECK(pref_name);
        // TODO(crbug.com/1455963): Find a better solution than manually
        // overriding the prefs' default values.
        // TODO(crbug.com/1455963): This should return true by default only if
        // a given type can actually run in transport mode.
        if (pref_service_->GetBoolean(pref_name) ||
            pref_service_->FindPreference(pref_name)->IsDefaultValue()) {
          // In transport-mode, individual types are considered enabled by
          // default.
#if BUILDFLAG(IS_IOS)
          // In transport-only mode, bookmarks and reading list require an
          // additional opt-in.
          // TODO(crbug.com/1440628): Cleanup the temporary behaviour of an
          // additional opt in for Bookmarks and Reading Lists.
          if (!base::FeatureList::IsEnabled(
                  kReplaceSyncPromosWithSignInPromos) &&
              (type == UserSelectableType::kBookmarks ||
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
            (!IsTypeManagedByPolicy(type) &&
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

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange();
  }
}

void SyncPrefs::SetSelectedType(UserSelectableType type, bool is_type_on) {
  pref_service_->SetBoolean(GetPrefNameForType(type), is_type_on);

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange();
  }
}

#if BUILDFLAG(IS_IOS)
void SyncPrefs::SetBookmarksAndReadingListAccountStorageOptIn(bool value) {
  pref_service_->SetBoolean(
      prefs::internal::kBookmarksAndReadingListAccountStorageOptIn, value);

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange();
  }
}

bool SyncPrefs::IsOptedInForBookmarksAndReadingListAccountStorageForTesting() {
  return pref_service_->GetBoolean(
      prefs::internal::kBookmarksAndReadingListAccountStorageOptIn);
}

void SyncPrefs::ClearBookmarksAndReadingListAccountStorageOptIn() {
  pref_service_->ClearPref(
      prefs::internal::kBookmarksAndReadingListAccountStorageOptIn);
}
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    observer.OnPreferredDataTypesPrefChange();
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
    observer.OnPreferredDataTypesPrefChange();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

bool SyncPrefs::IsSyncClientDisabledByPolicy() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::internal::kSyncManaged);
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
      // kSyncTypedUrls used here for historic reasons and pref backward
      // compatibility.
      return prefs::internal::kSyncTypedUrls;
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
  }
  NOTREACHED();
  return nullptr;
}

// static
void SyncPrefs::SetTypeDisabledByPolicy(PrefValueMap* policy_prefs,
                                        UserSelectableType type) {
  const char* pref_name = syncer::SyncPrefs::GetPrefNameForType(type);
  CHECK(pref_name);
  policy_prefs->SetValue(pref_name, base::Value(false));
}

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnSyncManagedPrefChange(*pref_sync_managed_);
}

void SyncPrefs::OnFirstSetupCompletePrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnFirstSetupCompletePrefChange(
        *pref_initial_sync_feature_setup_complete_);
}

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

void SyncPrefs::MaybeMigratePrefsForReplacingSyncWithSignin(
    SyncAccountState account_state) {
  if (!base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos)) {
    // Ensure that the migration runs again when the feature gets enabled.
    pref_service_->ClearPref(kReplacingSyncWithSigninMigrated);
    return;
  }

  // Don't migrate again if this profile was previously migrated.
  if (pref_service_->GetBoolean(kReplacingSyncWithSigninMigrated)) {
    return;
  }
  pref_service_->SetBoolean(kReplacingSyncWithSigninMigrated, true);

  switch (account_state) {
    case SyncAccountState::kNotSignedIn:
    case SyncAccountState::kSyncing: {
      // Nothing to migrate for signed-out or syncing users.
      break;
    }
    case SyncAccountState::kSignedInNotSyncing: {
      // For pre-existing signed-in users, some state needs to be migrated:

      // Settings aka preferences remains off by default.
      pref_service_->SetBoolean(
          GetPrefNameForType(UserSelectableType::kPreferences), false);

      // Addresses remains enabled only if the user didn't opt out for
      // passwords. Note that the pref being its default value (not explicitly
      // set) is treated as "not opted out"; see similar logic in
      // GetSelectedTypes().
      // TODO(crbug.com/1455963): Find a better solution than manually
      // overriding the pref's default value.
      const char* kPasswordsPref =
          GetPrefNameForType(UserSelectableType::kPasswords);
      if (!pref_service_->GetBoolean(kPasswordsPref) &&
          !pref_service_->FindPreference(kPasswordsPref)->IsDefaultValue()) {
        pref_service_->SetBoolean(
            GetPrefNameForType(UserSelectableType::kAutofill), false);
      }

#if BUILDFLAG(IS_IOS)
      // Bookmarks and reading list remain enabled only if the user previously
      // explicitly opted in.
      if (!pref_service_->GetBoolean(
              prefs::internal::kBookmarksAndReadingListAccountStorageOptIn)) {
        pref_service_->SetBoolean(
            GetPrefNameForType(UserSelectableType::kBookmarks), false);
        pref_service_->SetBoolean(
            GetPrefNameForType(UserSelectableType::kReadingList), false);
      }
#endif  // BUILDFLAG(IS_IOS)

      break;
    }
  }
}

}  // namespace syncer

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <utility>

#include "base/check_op.h"
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
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"

namespace syncer {

SyncPrefObserver::~SyncPrefObserver() = default;

SyncPrefs::SyncPrefs(PrefService* pref_service) : pref_service_(pref_service) {
  DCHECK(pref_service);
  // Watch the preference that indicates sync is managed so we can take
  // appropriate action.
  pref_sync_managed_.Init(
      prefs::kSyncManaged, pref_service_,
      base::BindRepeating(&SyncPrefs::OnSyncManagedPrefChanged,
                          base::Unretained(this)));
  pref_first_setup_complete_.Init(
      prefs::kSyncFirstSetupComplete, pref_service_,
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
  registry->RegisterBooleanPref(prefs::kSyncFirstSetupComplete, false);
  registry->RegisterBooleanPref(prefs::kSyncRequested, false);
  registry->RegisterBooleanPref(prefs::kSyncKeepEverythingSynced, true);
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    RegisterTypeSelectedPref(registry, type);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(prefs::kOsSyncPrefsMigrated, false);
  registry->RegisterBooleanPref(prefs::kSyncAllOsTypes, true);
  registry->RegisterBooleanPref(prefs::kSyncOsApps, false);
  registry->RegisterBooleanPref(prefs::kSyncOsPreferences, false);
  // The pref for Wi-Fi configurations is registered in the loop above.
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  registry->RegisterBooleanPref(prefs::kSyncAppsEnabledByOs, false);
#endif

  // The encryption bootstrap token represents a user-entered passphrase.
  registry->RegisterStringPref(prefs::kSyncEncryptionBootstrapToken,
                               std::string());

  registry->RegisterBooleanPref(prefs::kSyncManaged, false);
  registry->RegisterIntegerPref(prefs::kSyncPassphrasePromptMutedProductVersion,
                                0);
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

bool SyncPrefs::IsFirstSetupComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncFirstSetupComplete);
}

void SyncPrefs::SetFirstSetupComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncFirstSetupComplete, true);
}

void SyncPrefs::ClearFirstSetupComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::kSyncFirstSetupComplete);
}

bool SyncPrefs::IsSyncRequested() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncRequested);
}

void SyncPrefs::SetSyncRequested(bool is_requested) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncRequested, is_requested);
}

bool SyncPrefs::IsSyncRequestedSetExplicitly() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // GetUserPrefValue() returns nullptr if there is no user-set value for this
  // pref (there might still be a non-default value, e.g. from a policy, but we
  // explicitly don't care about that here).
  return pref_service_->GetUserPrefValue(prefs::kSyncRequested) != nullptr;
}

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced);
}

UserSelectableTypeSet SyncPrefs::GetSelectedTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UserSelectableTypeSet selected_types;

  const bool sync_all_types =
      pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced);

  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    const char* pref_name = GetPrefNameForType(type);
    DCHECK(pref_name);
    // If the type is managed, |sync_all_types| is ignored for this type.
    if (pref_service_->GetBoolean(pref_name) ||
        (sync_all_types && !IsTypeManagedByPolicy(type))) {
      selected_types.Put(type);
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

  pref_service_->SetBoolean(prefs::kSyncKeepEverythingSynced,
                            keep_everything_synced);

  for (UserSelectableType type : registered_types) {
    const char* pref_name = GetPrefNameForType(type);
    pref_service_->SetBoolean(pref_name, selected_types.Has(type));
  }

  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange();
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool SyncPrefs::IsSyncAllOsTypesEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncAllOsTypes);
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
  pref_service_->SetBoolean(prefs::kSyncAllOsTypes, sync_all_os_types);
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
const char* SyncPrefs::GetPrefNameForOsType(UserSelectableOsType type) {
  switch (type) {
    case UserSelectableOsType::kOsApps:
      return prefs::kSyncOsApps;
    case UserSelectableOsType::kOsPreferences:
      return prefs::kSyncOsPreferences;
    case UserSelectableOsType::kOsWifiConfigurations:
      return prefs::kSyncWifiConfigurations;
  }
  NOTREACHED();
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool SyncPrefs::IsAppsSyncEnabledByOs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncAppsEnabledByOs);
}

void SyncPrefs::SetAppsSyncEnabledByOs(bool apps_sync_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncAppsEnabledByOs, apps_sync_enabled);
  for (SyncPrefObserver& observer : sync_pref_observers_) {
    observer.OnPreferredDataTypesPrefChange();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

bool SyncPrefs::IsSyncClientDisabledByPolicy() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncManaged);
}

std::string SyncPrefs::GetEncryptionBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(prefs::kSyncEncryptionBootstrapToken);
}

void SyncPrefs::SetEncryptionBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncEncryptionBootstrapToken, token);
}

void SyncPrefs::ClearEncryptionBootstrapToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::kSyncEncryptionBootstrapToken);
}

// static
const char* SyncPrefs::GetPrefNameForType(UserSelectableType type) {
  switch (type) {
    case UserSelectableType::kBookmarks:
      return prefs::kSyncBookmarks;
    case UserSelectableType::kPreferences:
      return prefs::kSyncPreferences;
    case UserSelectableType::kPasswords:
      return prefs::kSyncPasswords;
    case UserSelectableType::kAutofill:
      return prefs::kSyncAutofill;
    case UserSelectableType::kThemes:
      return prefs::kSyncThemes;
    case UserSelectableType::kHistory:
      // kSyncTypedUrls used here for historic reasons and pref backward
      // compatibility.
      return prefs::kSyncTypedUrls;
    case UserSelectableType::kExtensions:
      return prefs::kSyncExtensions;
    case UserSelectableType::kApps:
      return prefs::kSyncApps;
    case UserSelectableType::kReadingList:
      return prefs::kSyncReadingList;
    case UserSelectableType::kTabs:
      return prefs::kSyncTabs;
    case UserSelectableType::kWifiConfigurations:
      return prefs::kSyncWifiConfigurations;
    case UserSelectableType::kSavedTabGroups:
      return prefs::kSyncSavedTabGroups;
  }
  NOTREACHED();
  return nullptr;
}

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnSyncManagedPrefChange(*pref_sync_managed_);
}

void SyncPrefs::OnFirstSetupCompletePrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnFirstSetupCompletePrefChange(*pref_first_setup_complete_);
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
      prefs::kSyncPassphrasePromptMutedProductVersion);
}

void SyncPrefs::SetPassphrasePromptMutedProductVersion(int major_version) {
  pref_service_->SetInteger(prefs::kSyncPassphrasePromptMutedProductVersion,
                            major_version);
}

void SyncPrefs::ClearPassphrasePromptMutedProductVersion() {
  pref_service_->ClearPref(prefs::kSyncPassphrasePromptMutedProductVersion);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void MigrateSyncRequestedPrefPostMice(PrefService* pref_service) {
  // Before MICe, there was a toggle in Sync settings that corresponded to the
  // SyncRequested bit. After MICe, there's no such toggle anymore, but some
  // users may still be in the legacy state where SyncRequested is false, for
  // various reasons:
  // * The original MICE implementation set SyncRequested to false if all data
  //   types were disabled, for migration / backwards compatibility reasons.
  //   This is no longer the case as of M104 (see crbug.com/1311270,
  //   crbug.com/1291946).
  // * On Android, users might have had the OS-level "auto sync" toggle
  //   disabled since before M90 or so (see crbug.com/1105795). Since then,
  //   Chrome does not integrate with the Android "auto sync" toggle anymore,
  //   but not all users were migrated.
  // Migrate all these users into a supported and equivalent state, where
  // SyncRequested is true but all data types are off.

  if (pref_service->GetBoolean(prefs::kSyncRequested) ||
      !pref_service->GetBoolean(prefs::kSyncFirstSetupComplete)) {
    // Either SyncRequested is already true, or FirstSetupComplete is false
    // meaning Sync isn't enabled. Either way, there's nothing to be done here.
    return;
  }

  // Disable all data types.
  pref_service->SetBoolean(prefs::kSyncKeepEverythingSynced, false);
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    pref_service->ClearPref(SyncPrefs::GetPrefNameForType(type));
  }

  // ...but turn on SyncRequested.
  pref_service->SetBoolean(prefs::kSyncRequested, true);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

}  // namespace syncer

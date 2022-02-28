// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
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

namespace {

// Obsolete pref that used to store if sync should be prevented from
// automatically starting up. This is now replaced by its inverse
// kSyncRequested.
const char kSyncSuppressStart[] = "sync.suppress_start";

#if BUILDFLAG(IS_ANDROID)
// Obsolete pref that used to store whether sync should no longer respect the
// state of the master toggle for this user. This is now always the case.
const char kObsoleteSyncDecoupledFromAndroidMasterSync[] =
    "sync.decoupled_from_master_sync";
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

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
  pref_sync_requested_.Init(
      prefs::kSyncRequested, pref_service_,
      base::BindRepeating(&SyncPrefs::OnSyncRequestedPrefChange,
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

  // The encryption bootstrap token represents a user-entered passphrase.
  registry->RegisterStringPref(prefs::kSyncEncryptionBootstrapToken,
                               std::string());

  registry->RegisterBooleanPref(prefs::kSyncManaged, false);
  registry->RegisterIntegerPref(prefs::kSyncPassphrasePromptMutedProductVersion,
                                0);
  registry->RegisterBooleanPref(prefs::kEnableLocalSyncBackend, false);
  registry->RegisterFilePathPref(prefs::kLocalSyncBackendDir, base::FilePath());

  // Obsolete prefs.
  registry->RegisterBooleanPref(kSyncSuppressStart, false);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(kObsoleteSyncDecoupledFromAndroidMasterSync,
                                false);
#endif  // BUILDFLAG(IS_ANDROID)
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

void SyncPrefs::SetSyncRequestedIfNotSetExplicitly() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // GetUserPrefValue() returns nullptr if there is no user-set value for this
  // pref (there might still be a non-default value, e.g. from a policy, but we
  // explicitly don't care about that here).
  if (!pref_service_->GetUserPrefValue(prefs::kSyncRequested)) {
    pref_service_->SetBoolean(prefs::kSyncRequested, true);
  }
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
    // If the preference is managed, |sync_all_types| is ignored for this
    // preference.
    if (pref_service_->GetBoolean(pref_name) ||
        (sync_all_types && !pref_service_->IsManagedPreference(pref_name))) {
      selected_types.Put(type);
    }
  }

  return selected_types;
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
  if (IsSyncAllOsTypesEnabled()) {
    return UserSelectableOsTypeSet::All();
  }
  UserSelectableOsTypeSet selected_types;
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    const char* pref_name = GetPrefNameForOsType(type);
    DCHECK(pref_name);
    if (pref_service_->GetBoolean(pref_name)) {
      selected_types.Put(type);
    }
  }
  return selected_types;
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

bool SyncPrefs::IsManaged() const {
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

void SyncPrefs::OnSyncRequestedPrefChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SyncPrefObserver& observer : sync_pref_observers_)
    observer.OnSyncRequestedPrefChange(*pref_sync_requested_);
}

void SyncPrefs::SetManagedForTest(bool is_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncManaged, is_managed);
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

#if BUILDFLAG(IS_ANDROID)
void ClearObsoleteSyncDecoupledFromAndroidMasterSync(
    PrefService* pref_service) {
  pref_service->ClearPref(kObsoleteSyncDecoupledFromAndroidMasterSync);
}
#endif  // BUILDFLAG(IS_ANDROID)

void MigrateSyncSuppressedPref(PrefService* pref_service) {
  // If the new kSyncRequested already has a value, there's nothing to be
  // done: Either the migration already happened, or we wrote to the new pref
  // directly.
  if (pref_service->GetUserPrefValue(prefs::kSyncRequested)) {
    return;
  }

  // If the old kSyncSuppressed has an explicit value, migrate it over.
  if (pref_service->GetUserPrefValue(kSyncSuppressStart)) {
    pref_service->SetBoolean(prefs::kSyncRequested,
                             !pref_service->GetBoolean(kSyncSuppressStart));
    pref_service->ClearPref(kSyncSuppressStart);
    DCHECK(pref_service->GetUserPrefValue(prefs::kSyncRequested));
    return;
  }

  // Neither old nor new pref have an explicit value. There should be nothing to
  // migrate, but it turns out some users are in a state that depends on the
  // implicit default value of the old pref (which was that Sync is NOT
  // suppressed, i.e. Sync is requested), see crbug.com/973770. To migrate these
  // users to the new pref correctly, use kSyncFirstSetupComplete as a signal
  // that Sync should be considered requested.
  if (pref_service->GetBoolean(prefs::kSyncFirstSetupComplete)) {
    // CHECK rather than DCHECK to make sure we never accidentally enable Sync
    // for users which had it previously disabled.
    CHECK(!pref_service->GetBoolean(kSyncSuppressStart));
    pref_service->SetBoolean(prefs::kSyncRequested, true);
    DCHECK(pref_service->GetUserPrefValue(prefs::kSyncRequested));
    return;
  }
  // Otherwise, nothing to be done: Sync was likely never enabled in this
  // profile.
}

}  // namespace syncer

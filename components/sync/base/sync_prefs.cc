// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"

namespace syncer {

namespace {

// Obsolete prefs related to the removed ClearServerData flow.
const char kSyncPassphraseEncryptionTransitionInProgress[] =
    "sync.passphrase_encryption_transition_in_progress";
const char kSyncNigoriStateForPassphraseTransition[] =
    "sync.nigori_state_for_passphrase_transition";

// Obsolete pref that used to store a bool on whether Sync has an auth error.
const char kSyncHasAuthError[] = "sync.has_auth_error";

// Obsolete pref that used to store the timestamp of first sync.
const char kSyncFirstSyncTime[] = "sync.first_sync_time";

// Obsolete pref that used to store long poll intervals received by the server.
const char kSyncLongPollIntervalSeconds[] = "sync.long_poll_interval";

// Obsolete pref that used to store if sync should be prevented from
// automatically starting up. This is now replaced by its inverse
// kSyncRequested.
const char kSyncSuppressStart[] = "sync.suppress_start";

// Obsolete prefs for data types. Can be deleted after 2020-01-30.
const char kSyncAppList[] = "sync.app_list";
const char kSyncAppNotifications[] = "sync.app_notifications";
const char kSyncAppSettings[] = "sync.app_settings";
const char kSyncArcPackage[] = "sync.arc_package";
const char kSyncArticles[] = "sync.articles";
const char kSyncAutofillProfile[] = "sync.autofill_profile";
const char kSyncAutofillWallet[] = "sync.autofill_wallet";
const char kSyncAutofillWalletMetadata[] = "sync.autofill_wallet_metadata";
const char kSyncDeviceInfo[] = "sync.device_info";
const char kSyncDictionary[] = "sync.dictionary";
const char kSyncExtensionSettings[] = "sync.extension_settings";
const char kSyncFaviconImages[] = "sync.favicon_images";
const char kSyncFaviconTracking[] = "sync.favicon_tracking";
const char kSyncHistoryDeleteDirectives[] = "sync.history_delete_directives";
const char kSyncMountainShares[] = "sync.mountain_shares";
const char kSyncPriorityPreferences[] = "sync.priority_preferences";
const char kSyncSearchEngines[] = "sync.search_engines";
const char kSyncSessions[] = "sync.sessions";
const char kSyncSupervisedUsers[] = "sync.managed_users";
const char kSyncSupervisedUserSettings[] = "sync.managed_user_settings";
const char kSyncSupervisedUserSharedSettings[] =
    "sync.managed_user_shared_settings";
const char kSyncSupervisedUserWhitelists[] = "sync.managed_user_whitelists";
const char kSyncSyncedNotificationAppInfo[] =
    "sync.synced_notification_app_info";
const char kSyncSyncedNotifications[] = "sync.synced_notifications";
const char kSyncUserEvents[] = "sync.user_events";
const char kSyncWifiCredentials[] = "sync.wifi_credentials";

// Obsolete pref. Can be deleted after 2020-09-09.
const char kSyncUserConsents[] = "sync.user_consents";

std::vector<std::string> GetObsoleteUserTypePrefs() {
  return {kSyncAutofillProfile,
          kSyncAutofillWallet,
          kSyncAutofillWalletMetadata,
          kSyncSearchEngines,
          kSyncSessions,
          kSyncAppSettings,
          kSyncExtensionSettings,
          kSyncAppNotifications,
          kSyncHistoryDeleteDirectives,
          kSyncSyncedNotifications,
          kSyncSyncedNotificationAppInfo,
          kSyncDictionary,
          kSyncFaviconImages,
          kSyncFaviconTracking,
          kSyncDeviceInfo,
          kSyncPriorityPreferences,
          kSyncSupervisedUserSettings,
          kSyncSupervisedUsers,
          kSyncSupervisedUserSharedSettings,
          kSyncArticles,
          kSyncAppList,
          kSyncWifiCredentials,
          kSyncSupervisedUserWhitelists,
          kSyncArcPackage,
          kSyncUserEvents,
          kSyncMountainShares,
          kSyncUserConsents};
}

void RegisterObsoleteUserTypePrefs(user_prefs::PrefRegistrySyncable* registry) {
  for (const std::string& obsolete_pref : GetObsoleteUserTypePrefs()) {
    registry->RegisterBooleanPref(obsolete_pref, false);
  }
}

// Gets an offset to add noise to the birth year. If not present in prefs, the
// offset will be randomly generated within the offset range and cached in
// syncable prefs.
int GetBirthYearOffset(PrefService* pref_service) {
  int offset =
      pref_service->GetInteger(prefs::kSyncDemographicsBirthYearOffset);
  if (offset == kUserDemographicsBirthYearNoiseOffsetDefaultValue) {
    // Generate a random offset when not cached in prefs.
    offset = base::RandInt(-kUserDemographicsBirthYearNoiseOffsetRange,
                           kUserDemographicsBirthYearNoiseOffsetRange);
    pref_service->SetInteger(prefs::kSyncDemographicsBirthYearOffset, offset);
  }
  return offset;
}

// Determines whether the synced user has provided a birth year to Google which
// is eligible, once aggregated and anonymized, to measure usage of Chrome
// features by age groups. See doc of metrics::DemographicMetricsProvider in
// components/metrics/demographic_metrics_provider.h for more details.
bool HasEligibleBirthYear(base::Time now, int user_birth_year, int offset) {
  // Compute user age.
  base::Time::Exploded exploded_now_time;
  now.LocalExplode(&exploded_now_time);
  int user_age = exploded_now_time.year - (user_birth_year + offset);

  // Verify if the synced user's age has a population size in the age
  // distribution of the society that is big enough to not raise the entropy of
  // the demographics too much. At a certain point, as the age increase, the
  // size of the population starts declining sharply as you can see in this
  // approximate representation of the age distribution:
  // |       ________         max age
  // |______/        \_________ |
  // |                          |\
  // |                          | \
  // +--------------------------|---------
  //  0 10 20 30 40 50 60 70 80 90 100+
  if (user_age > kUserDemographicsMaxAgeInYears)
    return false;

  // Verify if the synced user is old enough. Use > rather than >= because we
  // want to be sure that the user is at least |kUserDemographicsMinAgeInYears|
  // without disclosing their birth date, which requires to add an extra year
  // margin to the minimal age to be safe. For example, if we are in 2019-07-10
  // (now) and the user was born in 1999-08-10, the user is not yet 20 years old
  // (minimal age) but we cannot know that because we only have access to the
  // year of the dates (2019 and 1999 respectively). If we make sure that the
  // minimal age (computed at year granularity) is at least 21, we are 100% sure
  // that the user will be at least 20 years old when providing the user’s birth
  // year and gender.
  return user_age > kUserDemographicsMinAgeInYears;
}

// Gets the synced user's birth year from synced prefs, see doc of
// metrics::DemographicMetricsProvider in
// components/metrics/demographic_metrics_provider.h for more details.
base::Optional<int> GetUserBirthYear(
    const base::DictionaryValue* demographics) {
  const base::Value* value =
      demographics->FindPath(prefs::kSyncDemographics_BirthYearPath);
  int birth_year = (value != nullptr && value->is_int())
                       ? value->GetInt()
                       : kUserDemographicsBirthYearDefaultValue;

  // Verify that there is a birth year.
  if (birth_year == kUserDemographicsBirthYearDefaultValue)
    return base::nullopt;

  return birth_year;
}

// Gets the synced user's gender from synced prefs, see doc of
// metrics::DemographicMetricsProvider in
// components/metrics/demographic_metrics_provider.h for more details.
base::Optional<metrics::UserDemographicsProto_Gender> GetUserGender(
    const base::DictionaryValue* demographics) {
  const base::Value* value =
      demographics->FindPath(prefs::kSyncDemographics_GenderPath);
  int gender_int = (value != nullptr && value->is_int())
                       ? value->GetInt()
                       : kUserDemographicsGenderDefaultValue;

  // Verify that the gender is not default.
  if (gender_int == kUserDemographicsGenderDefaultValue)
    return base::nullopt;

  // Verify that the gender number is a valid UserDemographicsProto_Gender
  // encoding.
  if (!metrics::UserDemographicsProto_Gender_IsValid(gender_int))
    return base::nullopt;

  auto gender = metrics::UserDemographicsProto_Gender(gender_int);

  // Verify that the gender is in a large enough population set to preserve
  // anonymity.
  if (gender != metrics::UserDemographicsProto::GENDER_FEMALE &&
      gender != metrics::UserDemographicsProto::GENDER_MALE) {
    return base::nullopt;
  }

  return gender;
}

}  // namespace

CryptoSyncPrefs::~CryptoSyncPrefs() {}

SyncPrefObserver::~SyncPrefObserver() {}

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
void SyncPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Actual user-controlled preferences.
  registry->RegisterBooleanPref(prefs::kSyncFirstSetupComplete, false);
  registry->RegisterBooleanPref(prefs::kSyncRequested, false);
  registry->RegisterBooleanPref(prefs::kSyncKeepEverythingSynced, true);
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    RegisterTypeSelectedPref(registry, type);
  }
#if defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kOsSyncPrefsMigrated, false);
  registry->RegisterBooleanPref(prefs::kOsSyncFeatureEnabled, false);
  registry->RegisterBooleanPref(prefs::kSyncAllOsTypes, true);
  registry->RegisterBooleanPref(prefs::kSyncOsApps, false);
  registry->RegisterBooleanPref(prefs::kSyncOsPreferences, false);
  // The pref for Wi-Fi configurations is registered in the loop above.
#endif

  // The encryption bootstrap token represents a user-entered passphrase.
  registry->RegisterStringPref(prefs::kSyncEncryptionBootstrapToken,
                               std::string());

  // Internal or bookkeeping prefs.
  registry->RegisterStringPref(prefs::kSyncGaiaId, std::string());
  registry->RegisterStringPref(prefs::kSyncCacheGuid, std::string());
  registry->RegisterStringPref(prefs::kSyncBirthday, std::string());
  registry->RegisterStringPref(prefs::kSyncBagOfChips, std::string());
  registry->RegisterInt64Pref(prefs::kSyncLastSyncedTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncLastPollTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncPollIntervalSeconds, 0);
  registry->RegisterBooleanPref(prefs::kSyncManaged, false);
  registry->RegisterStringPref(prefs::kSyncKeystoreEncryptionBootstrapToken,
                               std::string());
  registry->RegisterBooleanPref(prefs::kSyncPassphrasePrompted, false);
  registry->RegisterDictionaryPref(prefs::kSyncInvalidationVersions);
  registry->RegisterStringPref(prefs::kSyncLastRunVersion, std::string());
  registry->RegisterBooleanPref(prefs::kEnableLocalSyncBackend, false);
  registry->RegisterFilePathPref(prefs::kLocalSyncBackendDir, base::FilePath());
#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kSyncDecoupledFromAndroidMasterSync,
                                false);
#endif  // defined(OS_ANDROID)

  // Demographic prefs.
  registry->RegisterDictionaryPref(
      prefs::kSyncDemographics,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kSyncDemographicsBirthYearOffset,
      kUserDemographicsBirthYearNoiseOffsetDefaultValue,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // Obsolete prefs that will be removed after a grace period.
  RegisterObsoleteUserTypePrefs(registry);
  registry->RegisterBooleanPref(kSyncPassphraseEncryptionTransitionInProgress,
                                false);
  registry->RegisterStringPref(kSyncNigoriStateForPassphraseTransition,
                               std::string());
  registry->RegisterBooleanPref(kSyncHasAuthError, false);
  registry->RegisterInt64Pref(kSyncFirstSyncTime, 0);
  registry->RegisterInt64Pref(kSyncLongPollIntervalSeconds, 0);
  registry->RegisterBooleanPref(kSyncSuppressStart, false);
}

void SyncPrefs::AddSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.AddObserver(sync_pref_observer);
}

void SyncPrefs::RemoveSyncPrefObserver(SyncPrefObserver* sync_pref_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sync_pref_observers_.RemoveObserver(sync_pref_observer);
}

void SyncPrefs::ClearLocalSyncTransportData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear user's birth year and gender.
  // Note that we retain kSyncDemographicsBirthYearOffset. If the user resumes
  // syncing, causing these prefs to be recreated, we don't want them to start
  // reporting a different randomized birth year as this could narrow down or
  // even reveal their true birth year.
  pref_service_->ClearPref(prefs::kSyncDemographics);

  pref_service_->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service_->ClearPref(prefs::kSyncLastPollTime);
  pref_service_->ClearPref(prefs::kSyncPollIntervalSeconds);
  pref_service_->ClearPref(prefs::kSyncKeystoreEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncPassphrasePrompted);
  pref_service_->ClearPref(prefs::kSyncInvalidationVersions);
  pref_service_->ClearPref(prefs::kSyncLastRunVersion);
  pref_service_->ClearPref(prefs::kSyncGaiaId);
  pref_service_->ClearPref(prefs::kSyncCacheGuid);
  pref_service_->ClearPref(prefs::kSyncBirthday);
  pref_service_->ClearPref(prefs::kSyncBagOfChips);
#if defined(OS_ANDROID)
  pref_service_->ClearPref(prefs::kSyncDecoupledFromAndroidMasterSync);
#endif  // defined(OS_ANDROID)

  // No need to clear kManaged, kEnableLocalSyncBackend or kLocalSyncBackendDir,
  // since they're never actually set as user preferences.
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

base::Time SyncPrefs::GetLastSyncedTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncLastSyncedTime));
}

void SyncPrefs::SetLastSyncedTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncLastSyncedTime, time.ToInternalValue());
}

base::Time SyncPrefs::GetLastPollTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncLastPollTime));
}

void SyncPrefs::SetLastPollTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncLastPollTime, time.ToInternalValue());
}

base::TimeDelta SyncPrefs::GetPollInterval() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::TimeDelta::FromSeconds(
      pref_service_->GetInt64(prefs::kSyncPollIntervalSeconds));
}

void SyncPrefs::SetPollInterval(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncPollIntervalSeconds,
                          interval.InSeconds());
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

#if defined(OS_CHROMEOS)
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

bool SyncPrefs::IsOsSyncFeatureEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kOsSyncFeatureEnabled);
}

void SyncPrefs::SetOsSyncFeatureEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kOsSyncFeatureEnabled, enabled);
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
#endif  // defined(OS_CHROMEOS)

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

std::string SyncPrefs::GetKeystoreEncryptionBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(prefs::kSyncKeystoreEncryptionBootstrapToken);
}

void SyncPrefs::SetKeystoreEncryptionBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncKeystoreEncryptionBootstrapToken, token);
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
void SyncPrefs::RegisterTypeSelectedPref(
    user_prefs::PrefRegistrySyncable* registry,
    UserSelectableType type) {
  const char* pref_name = GetPrefNameForType(type);
  DCHECK(pref_name);
  registry->RegisterBooleanPref(pref_name, false);
}

void SyncPrefs::SetGaiaId(const std::string& gaia_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncGaiaId, gaia_id);
}

std::string SyncPrefs::GetGaiaId() const {
  return pref_service_->GetString(prefs::kSyncGaiaId);
}

void SyncPrefs::SetCacheGuid(const std::string& cache_guid) {
  pref_service_->SetString(prefs::kSyncCacheGuid, cache_guid);
}

std::string SyncPrefs::GetCacheGuid() const {
  return pref_service_->GetString(prefs::kSyncCacheGuid);
}

void SyncPrefs::SetBirthday(const std::string& birthday) {
  pref_service_->SetString(prefs::kSyncBirthday, birthday);
}

std::string SyncPrefs::GetBirthday() const {
  return pref_service_->GetString(prefs::kSyncBirthday);
}

void SyncPrefs::SetBagOfChips(const std::string& bag_of_chips) {
  // |bag_of_chips| contains a serialized proto which is not utf-8, hence we use
  // base64 encoding in prefs.
  std::string encoded;
  base::Base64Encode(bag_of_chips, &encoded);
  pref_service_->SetString(prefs::kSyncBagOfChips, encoded);
}

std::string SyncPrefs::GetBagOfChips() const {
  // |kSyncBagOfChips| gets stored in base64 because it represents a serialized
  // proto which is not utf-8 encoding.
  const std::string encoded = pref_service_->GetString(prefs::kSyncBagOfChips);
  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  return decoded;
}

bool SyncPrefs::IsPassphrasePrompted() const {
  return pref_service_->GetBoolean(prefs::kSyncPassphrasePrompted);
}

void SyncPrefs::SetPassphrasePrompted(bool value) {
  pref_service_->SetBoolean(prefs::kSyncPassphrasePrompted, value);
}

#if defined(OS_ANDROID)
void SyncPrefs::SetDecoupledFromAndroidMasterSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncDecoupledFromAndroidMasterSync, true);
}

bool SyncPrefs::GetDecoupledFromAndroidMasterSync() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncDecoupledFromAndroidMasterSync);
}
#endif  // defined(OS_ANDROID)

std::map<ModelType, int64_t> SyncPrefs::GetInvalidationVersions() const {
  std::map<ModelType, int64_t> invalidation_versions;
  const base::DictionaryValue* invalidation_dictionary =
      pref_service_->GetDictionary(prefs::kSyncInvalidationVersions);
  for (ModelType type : ProtocolTypes()) {
    std::string key = ModelTypeToString(type);
    std::string version_str;
    if (!invalidation_dictionary->GetString(key, &version_str))
      continue;
    int64_t version = 0;
    if (!base::StringToInt64(version_str, &version))
      continue;
    invalidation_versions[type] = version;
  }
  return invalidation_versions;
}

void SyncPrefs::UpdateInvalidationVersions(
    const std::map<ModelType, int64_t>& invalidation_versions) {
  std::unique_ptr<base::DictionaryValue> invalidation_dictionary(
      new base::DictionaryValue());
  for (const auto& map_iter : invalidation_versions) {
    std::string version_str = base::NumberToString(map_iter.second);
    invalidation_dictionary->SetString(ModelTypeToString(map_iter.first),
                                       version_str);
  }
  pref_service_->Set(prefs::kSyncInvalidationVersions,
                     *invalidation_dictionary);
}

std::string SyncPrefs::GetLastRunVersion() const {
  return pref_service_->GetString(prefs::kSyncLastRunVersion);
}

void SyncPrefs::SetLastRunVersion(const std::string& current_version) {
  pref_service_->SetString(prefs::kSyncLastRunVersion, current_version);
}

bool SyncPrefs::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

UserDemographicsResult SyncPrefs::GetUserNoisedBirthYearAndGender(
    base::Time now) {
  // Verify that the now time is available. There are situations where the now
  // time cannot be provided.
  if (now.is_null()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kCannotGetTime);
  }

  // Get the synced user’s noised birth year and gender from synced prefs. Only
  // one error status code should be used to represent the case where
  // demographics are ineligible, see doc of UserDemographicsStatus in
  // components/sync/base/user_demographics.h for more details.

  // Get the pref that contains the user's birth year and gender.
  const base::DictionaryValue* demographics =
      pref_service_->GetDictionary(prefs::kSyncDemographics);
  DCHECK(demographics != nullptr);

  // Get the user's birth year.
  base::Optional<int> birth_year = GetUserBirthYear(demographics);
  if (!birth_year.has_value()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kIneligibleDemographicsData);
  }

  // Get the user's gender.
  base::Optional<metrics::UserDemographicsProto_Gender> gender =
      GetUserGender(demographics);
  if (!gender.has_value()) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kIneligibleDemographicsData);
  }

  // Get the offset and do one last check that the birth year is eligible.
  int offset = GetBirthYearOffset(pref_service_);
  if (!HasEligibleBirthYear(now, *birth_year, offset)) {
    return UserDemographicsResult::ForStatus(
        UserDemographicsStatus::kIneligibleDemographicsData);
  }

  // Set gender and noised birth year in demographics.
  UserDemographics user_demographics;
  user_demographics.gender = *gender;
  user_demographics.birth_year = *birth_year + offset;

  return UserDemographicsResult::ForValue(std::move(user_demographics));
}

void MigrateSessionsToProxyTabsPrefs(PrefService* pref_service) {
  if (pref_service->GetUserPrefValue(prefs::kSyncTabs) == nullptr &&
      pref_service->GetUserPrefValue(kSyncSessions) != nullptr &&
      pref_service->IsUserModifiablePreference(prefs::kSyncTabs)) {
    // If there is no tab sync preference yet (i.e. newly enabled type),
    // default to the session sync preference value.
    bool sessions_pref_value = pref_service->GetBoolean(kSyncSessions);
    pref_service->SetBoolean(prefs::kSyncTabs, sessions_pref_value);
  }
}

void ClearObsoleteUserTypePrefs(PrefService* pref_service) {
  for (const std::string& obsolete_pref : GetObsoleteUserTypePrefs()) {
    pref_service->ClearPref(obsolete_pref);
  }
}

void ClearObsoleteClearServerDataPrefs(PrefService* pref_service) {
  pref_service->ClearPref(kSyncPassphraseEncryptionTransitionInProgress);
  pref_service->ClearPref(kSyncNigoriStateForPassphraseTransition);
}

void ClearObsoleteAuthErrorPrefs(PrefService* pref_service) {
  pref_service->ClearPref(kSyncHasAuthError);
}

void ClearObsoleteFirstSyncTime(PrefService* pref_service) {
  pref_service->ClearPref(kSyncFirstSyncTime);
}

void ClearObsoleteSyncLongPollIntervalSeconds(PrefService* pref_service) {
  pref_service->ClearPref(kSyncLongPollIntervalSeconds);
}


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

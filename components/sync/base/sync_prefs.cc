// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"

namespace syncer {

namespace {
// Groups of prefs that always have the same value as a "master" pref.
// For example, the APPS group has {APP_NOTIFICATIONS, APP_SETTINGS}
// (as well as APPS, but that is implied), so
//   pref_groups_[APPS] =       { APP_NOTIFICATIONS,
//                                          APP_SETTINGS }
//   pref_groups_[EXTENSIONS] = { EXTENSION_SETTINGS }
// etc.
using PrefGroupsMap = std::map<ModelType, ModelTypeSet>;
PrefGroupsMap ComputePrefGroups(bool user_events_separate_pref_group) {
  PrefGroupsMap pref_groups;
  pref_groups[APPS].Put(APP_NOTIFICATIONS);
  pref_groups[APPS].Put(APP_SETTINGS);
  pref_groups[APPS].Put(APP_LIST);
  pref_groups[APPS].Put(ARC_PACKAGE);
  pref_groups[APPS].Put(READING_LIST);

  pref_groups[AUTOFILL].Put(AUTOFILL_PROFILE);
  pref_groups[AUTOFILL].Put(AUTOFILL_WALLET_DATA);
  pref_groups[AUTOFILL].Put(AUTOFILL_WALLET_METADATA);

  pref_groups[EXTENSIONS].Put(EXTENSION_SETTINGS);

  pref_groups[PREFERENCES].Put(DICTIONARY);
  pref_groups[PREFERENCES].Put(PRIORITY_PREFERENCES);
  pref_groups[PREFERENCES].Put(SEARCH_ENGINES);

  pref_groups[TYPED_URLS].Put(HISTORY_DELETE_DIRECTIVES);
  pref_groups[TYPED_URLS].Put(SESSIONS);
  pref_groups[TYPED_URLS].Put(FAVICON_IMAGES);
  pref_groups[TYPED_URLS].Put(FAVICON_TRACKING);

  if (!user_events_separate_pref_group) {
    pref_groups[TYPED_URLS].Put(USER_EVENTS);
  }

  pref_groups[PROXY_TABS].Put(SESSIONS);
  pref_groups[PROXY_TABS].Put(FAVICON_IMAGES);
  pref_groups[PROXY_TABS].Put(FAVICON_TRACKING);

  // TODO(zea): Put favicons in the bookmarks group as well once it handles
  // those favicons.

  return pref_groups;
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
      base::Bind(&SyncPrefs::OnSyncManagedPrefChanged, base::Unretained(this)));
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
  registry->RegisterBooleanPref(prefs::kSyncFirstSetupComplete, false);
  registry->RegisterBooleanPref(prefs::kSyncSuppressStart, false);
  registry->RegisterInt64Pref(prefs::kSyncLastSyncedTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncLastPollTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncFirstSyncTime, 0);
  registry->RegisterInt64Pref(prefs::kSyncShortPollIntervalSeconds, 0);
  registry->RegisterInt64Pref(prefs::kSyncLongPollIntervalSeconds, 0);

  // All datatypes are on by default, but this gets set explicitly
  // when you configure sync (when turning it on), in
  // ProfileSyncService::OnUserChoseDatatypes.
  registry->RegisterBooleanPref(prefs::kSyncKeepEverythingSynced, true);

  ModelTypeSet user_types = UserTypes();

  // Include proxy types as well, as they can be individually selected,
  // although they don't have sync representations.
  user_types.PutAll(ProxyTypes());

  // All types except the always-preferred ones are set to off by default, which
  // forces a configuration to explicitly enable them. GetPreferredTypes() will
  // ensure that any new implicit types are enabled when their pref group is, or
  // via KeepEverythingSynced.
  for (ModelType type : user_types) {
    RegisterDataTypePreferredPref(registry, type,
                                  AlwaysPreferredUserTypes().Has(type));
  }

  registry->RegisterBooleanPref(prefs::kSyncManaged, false);
  registry->RegisterStringPref(prefs::kSyncEncryptionBootstrapToken,
                               std::string());
  registry->RegisterStringPref(prefs::kSyncKeystoreEncryptionBootstrapToken,
                               std::string());
#if defined(OS_CHROMEOS)
  registry->RegisterStringPref(prefs::kSyncSpareBootstrapToken, "");
#endif

  registry->RegisterBooleanPref(prefs::kSyncHasAuthError, false);
  registry->RegisterBooleanPref(prefs::kSyncPassphrasePrompted, false);
  registry->RegisterIntegerPref(prefs::kSyncMemoryPressureWarningCount, -1);
  registry->RegisterBooleanPref(prefs::kSyncShutdownCleanly, false);
  registry->RegisterDictionaryPref(prefs::kSyncInvalidationVersions);
  registry->RegisterStringPref(prefs::kSyncLastRunVersion, std::string());
  registry->RegisterBooleanPref(
      prefs::kSyncPassphraseEncryptionTransitionInProgress, false);
  registry->RegisterStringPref(prefs::kSyncNigoriStateForPassphraseTransition,
                               std::string());
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

void SyncPrefs::ClearPreferences() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::kSyncLastSyncedTime);
  pref_service_->ClearPref(prefs::kSyncLastPollTime);
  pref_service_->ClearPref(prefs::kSyncShortPollIntervalSeconds);
  pref_service_->ClearPref(prefs::kSyncLongPollIntervalSeconds);
  pref_service_->ClearPref(prefs::kSyncFirstSetupComplete);
  pref_service_->ClearPref(prefs::kSyncEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncKeystoreEncryptionBootstrapToken);
  pref_service_->ClearPref(prefs::kSyncPassphrasePrompted);
  pref_service_->ClearPref(prefs::kSyncMemoryPressureWarningCount);
  pref_service_->ClearPref(prefs::kSyncShutdownCleanly);
  pref_service_->ClearPref(prefs::kSyncInvalidationVersions);
  pref_service_->ClearPref(prefs::kSyncLastRunVersion);
  pref_service_->ClearPref(
      prefs::kSyncPassphraseEncryptionTransitionInProgress);
  pref_service_->ClearPref(prefs::kSyncNigoriStateForPassphraseTransition);

  // Note: We do *not* clear prefs which are directly user-controlled such as
  // the set of preferred data types here.
}

bool SyncPrefs::IsFirstSetupComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncFirstSetupComplete);
}

void SyncPrefs::SetFirstSetupComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncFirstSetupComplete, true);
}

bool SyncPrefs::SyncHasAuthError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncHasAuthError);
}

void SyncPrefs::SetSyncAuthError(bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncHasAuthError, error);
}

bool SyncPrefs::IsSyncRequested() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // IsSyncRequested is the inverse of the old SuppressStart pref.
  // Since renaming a pref value is hard, here we still use the old one.
  return !pref_service_->GetBoolean(prefs::kSyncSuppressStart);
}

void SyncPrefs::SetSyncRequested(bool is_requested) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // See IsSyncRequested for why we use this pref and !is_requested.
  pref_service_->SetBoolean(prefs::kSyncSuppressStart, !is_requested);
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

base::TimeDelta SyncPrefs::GetShortPollInterval() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::TimeDelta::FromSeconds(
      pref_service_->GetInt64(prefs::kSyncShortPollIntervalSeconds));
}

void SyncPrefs::SetShortPollInterval(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncShortPollIntervalSeconds,
                          interval.InSeconds());
}

base::TimeDelta SyncPrefs::GetLongPollInterval() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::TimeDelta::FromSeconds(
      pref_service_->GetInt64(prefs::kSyncLongPollIntervalSeconds));
}

void SyncPrefs::SetLongPollInterval(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(prefs::kSyncLongPollIntervalSeconds,
                          interval.InSeconds());
}

bool SyncPrefs::HasKeepEverythingSynced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced);
}

void SyncPrefs::SetKeepEverythingSynced(bool keep_everything_synced) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncKeepEverythingSynced,
                            keep_everything_synced);
}

ModelTypeSet SyncPrefs::GetPreferredDataTypes(
    ModelTypeSet registered_types,
    bool user_events_separate_pref_group) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pref_service_->GetBoolean(prefs::kSyncKeepEverythingSynced)) {
    return registered_types;
  }

  ModelTypeSet preferred_types;
  for (ModelType type : registered_types) {
    if (GetDataTypePreferred(type)) {
      preferred_types.Put(type);
    }
  }
  return ResolvePrefGroups(registered_types, preferred_types,
                           user_events_separate_pref_group);
}

void SyncPrefs::SetPreferredDataTypes(ModelTypeSet registered_types,
                                      ModelTypeSet preferred_types,
                                      bool user_events_separate_pref_group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  preferred_types = ResolvePrefGroups(registered_types, preferred_types,
                                      user_events_separate_pref_group);
  DCHECK(registered_types.HasAll(preferred_types));
  for (ModelType type : registered_types) {
    SetDataTypePreferred(type, preferred_types.Has(type));
  }
}

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

std::string SyncPrefs::GetKeystoreEncryptionBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(prefs::kSyncKeystoreEncryptionBootstrapToken);
}

void SyncPrefs::SetKeystoreEncryptionBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncKeystoreEncryptionBootstrapToken, token);
}

// static
const char* SyncPrefs::GetPrefNameForDataType(ModelType type) {
  switch (type) {
    case UNSPECIFIED:
    case TOP_LEVEL_FOLDER:
      break;
    case BOOKMARKS:
      return prefs::kSyncBookmarks;
    case PREFERENCES:
      return prefs::kSyncPreferences;
    case PASSWORDS:
      return prefs::kSyncPasswords;
    case AUTOFILL_PROFILE:
      return prefs::kSyncAutofillProfile;
    case AUTOFILL:
      return prefs::kSyncAutofill;
    case AUTOFILL_WALLET_DATA:
      return prefs::kSyncAutofillWallet;
    case AUTOFILL_WALLET_METADATA:
      return prefs::kSyncAutofillWalletMetadata;
    case THEMES:
      return prefs::kSyncThemes;
    case TYPED_URLS:
      return prefs::kSyncTypedUrls;
    case EXTENSIONS:
      return prefs::kSyncExtensions;
    case SEARCH_ENGINES:
      return prefs::kSyncSearchEngines;
    case SESSIONS:
      return prefs::kSyncSessions;
    case APPS:
      return prefs::kSyncApps;
    case APP_SETTINGS:
      return prefs::kSyncAppSettings;
    case EXTENSION_SETTINGS:
      return prefs::kSyncExtensionSettings;
    case APP_NOTIFICATIONS:
      return prefs::kSyncAppNotifications;
    case HISTORY_DELETE_DIRECTIVES:
      return prefs::kSyncHistoryDeleteDirectives;
    case SYNCED_NOTIFICATIONS:
      return prefs::kSyncSyncedNotifications;
    case SYNCED_NOTIFICATION_APP_INFO:
      return prefs::kSyncSyncedNotificationAppInfo;
    case DICTIONARY:
      return prefs::kSyncDictionary;
    case FAVICON_IMAGES:
      return prefs::kSyncFaviconImages;
    case FAVICON_TRACKING:
      return prefs::kSyncFaviconTracking;
    case DEVICE_INFO:
      return prefs::kSyncDeviceInfo;
    case PRIORITY_PREFERENCES:
      return prefs::kSyncPriorityPreferences;
    case SUPERVISED_USER_SETTINGS:
      return prefs::kSyncSupervisedUserSettings;
    case DEPRECATED_SUPERVISED_USERS:
      return prefs::kSyncSupervisedUsers;
    case DEPRECATED_SUPERVISED_USER_SHARED_SETTINGS:
      return prefs::kSyncSupervisedUserSharedSettings;
    case DEPRECATED_ARTICLES:
      return prefs::kSyncArticles;
    case APP_LIST:
      return prefs::kSyncAppList;
    case WIFI_CREDENTIALS:
      return prefs::kSyncWifiCredentials;
    case SUPERVISED_USER_WHITELISTS:
      return prefs::kSyncSupervisedUserWhitelists;
    case ARC_PACKAGE:
      return prefs::kSyncArcPackage;
    case PRINTERS:
      return prefs::kSyncPrinters;
    case READING_LIST:
      return prefs::kSyncReadingList;
    case USER_EVENTS:
      return prefs::kSyncUserEvents;
    case PROXY_TABS:
      return prefs::kSyncTabs;
    case MOUNTAIN_SHARES:
      return prefs::kSyncMountainShares;
    case USER_CONSENTS:
      return prefs::kSyncUserConsents;
    case NIGORI:
    case EXPERIMENTS:
    case MODEL_TYPE_COUNT:
      break;
  }
  NOTREACHED() << "No pref mapping for type " << ModelTypeToString(type);
  return nullptr;
}

#if defined(OS_CHROMEOS)
std::string SyncPrefs::GetSpareBootstrapToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(prefs::kSyncSpareBootstrapToken);
}

void SyncPrefs::SetSpareBootstrapToken(const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(prefs::kSyncSpareBootstrapToken, token);
}
#endif

void SyncPrefs::OnSyncManagedPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : sync_pref_observers_)
    observer.OnSyncManagedPrefChange(*pref_sync_managed_);
}

void SyncPrefs::SetManagedForTest(bool is_managed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetBoolean(prefs::kSyncManaged, is_managed);
}

// static
void SyncPrefs::RegisterDataTypePreferredPref(
    user_prefs::PrefRegistrySyncable* registry,
    ModelType type,
    bool is_preferred) {
  const char* pref_name = GetPrefNameForDataType(type);
  DCHECK(pref_name);
  registry->RegisterBooleanPref(pref_name, is_preferred);
}

bool SyncPrefs::GetDataTypePreferred(ModelType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const char* pref_name = GetPrefNameForDataType(type);
  DCHECK(pref_name);

  if (AlwaysPreferredUserTypes().Has(type))
    return true;

  if (type == PROXY_TABS &&
      pref_service_->GetUserPrefValue(pref_name) == nullptr &&
      pref_service_->IsUserModifiablePreference(pref_name)) {
    // If there is no tab sync preference yet (i.e. newly enabled type),
    // default to the session sync preference value.
    pref_name = GetPrefNameForDataType(SESSIONS);
  }

  return pref_service_->GetBoolean(pref_name);
}

void SyncPrefs::SetDataTypePreferred(ModelType type, bool is_preferred) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const char* pref_name = GetPrefNameForDataType(type);
  DCHECK(pref_name);

  if (AlwaysPreferredUserTypes().Has(type))
    return;

  pref_service_->SetBoolean(pref_name, is_preferred);
}

// static
ModelTypeSet SyncPrefs::ResolvePrefGroups(
    ModelTypeSet registered_types,
    ModelTypeSet types,
    bool user_events_separate_pref_group) {
  ModelTypeSet types_with_groups = types;
  for (const auto& pref_group :
       ComputePrefGroups(user_events_separate_pref_group)) {
    if (types.Has(pref_group.first)) {
      types_with_groups.PutAll(pref_group.second);
    }
  }
  types_with_groups.RetainAll(registered_types);
  return types_with_groups;
}

base::Time SyncPrefs::GetFirstSyncTime() const {
  return base::Time::FromInternalValue(
      pref_service_->GetInt64(prefs::kSyncFirstSyncTime));
}

void SyncPrefs::SetFirstSyncTime(base::Time time) {
  pref_service_->SetInt64(prefs::kSyncFirstSyncTime, time.ToInternalValue());
}

void SyncPrefs::ClearFirstSyncTime() {
  pref_service_->ClearPref(prefs::kSyncFirstSyncTime);
}

bool SyncPrefs::IsPassphrasePrompted() const {
  return pref_service_->GetBoolean(prefs::kSyncPassphrasePrompted);
}

void SyncPrefs::SetPassphrasePrompted(bool value) {
  pref_service_->SetBoolean(prefs::kSyncPassphrasePrompted, value);
}

int SyncPrefs::GetMemoryPressureWarningCount() const {
  return pref_service_->GetInteger(prefs::kSyncMemoryPressureWarningCount);
}

void SyncPrefs::SetMemoryPressureWarningCount(int value) {
  pref_service_->SetInteger(prefs::kSyncMemoryPressureWarningCount, value);
}

bool SyncPrefs::DidSyncShutdownCleanly() const {
  return pref_service_->GetBoolean(prefs::kSyncShutdownCleanly);
}

void SyncPrefs::SetCleanShutdown(bool value) {
  pref_service_->SetBoolean(prefs::kSyncShutdownCleanly, value);
}

void SyncPrefs::GetInvalidationVersions(
    std::map<ModelType, int64_t>* invalidation_versions) const {
  const base::DictionaryValue* invalidation_dictionary =
      pref_service_->GetDictionary(prefs::kSyncInvalidationVersions);
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    std::string key = ModelTypeToString(type);
    std::string version_str;
    if (!invalidation_dictionary->GetString(key, &version_str))
      continue;
    int64_t version = 0;
    if (!base::StringToInt64(version_str, &version))
      continue;
    (*invalidation_versions)[type] = version;
  }
}

void SyncPrefs::UpdateInvalidationVersions(
    const std::map<ModelType, int64_t>& invalidation_versions) {
  std::unique_ptr<base::DictionaryValue> invalidation_dictionary(
      new base::DictionaryValue());
  for (const auto& map_iter : invalidation_versions) {
    std::string version_str = base::Int64ToString(map_iter.second);
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

void SyncPrefs::SetPassphraseEncryptionTransitionInProgress(bool value) {
  pref_service_->SetBoolean(
      prefs::kSyncPassphraseEncryptionTransitionInProgress, value);
}

bool SyncPrefs::GetPassphraseEncryptionTransitionInProgress() const {
  return pref_service_->GetBoolean(
      prefs::kSyncPassphraseEncryptionTransitionInProgress);
}

void SyncPrefs::SetNigoriSpecificsForPassphraseTransition(
    const sync_pb::NigoriSpecifics& nigori_specifics) {
  std::string encoded;
  base::Base64Encode(nigori_specifics.SerializeAsString(), &encoded);
  pref_service_->SetString(prefs::kSyncNigoriStateForPassphraseTransition,
                           encoded);
}

void SyncPrefs::GetNigoriSpecificsForPassphraseTransition(
    sync_pb::NigoriSpecifics* nigori_specifics) const {
  const std::string encoded =
      pref_service_->GetString(prefs::kSyncNigoriStateForPassphraseTransition);
  std::string decoded;
  if (base::Base64Decode(encoded, &decoded)) {
    nigori_specifics->ParseFromString(decoded);
  }
}

bool SyncPrefs::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

}  // namespace syncer

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_transport_data_prefs.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"

namespace syncer {

namespace {

// 64-bit integer serialization of the base::Time when the last sync occurred.
const char kSyncLastSyncedTime[] = "sync.last_synced_time";

// 64-bit integer serialization of the base::Time of the last sync poll.
const char kSyncLastPollTime[] = "sync.last_poll_time";

// 64-bit integer serialization of base::TimeDelta storing poll intervals
// received by the server (in seconds). For historic reasons, this is called
// "short_poll_interval", but it's not worth the hassle to rename it.
const char kSyncPollIntervalSeconds[] = "sync.short_poll_interval";

const char kSyncGaiaId[] = "sync.gaia_id";
const char kSyncCacheGuid[] = "sync.cache_guid";
const char kSyncBirthday[] = "sync.birthday";
const char kSyncBagOfChips[] = "sync.bag_of_chips";

// Dictionary of last seen invalidation versions for each model type.
const char kDeprecatedSyncInvalidationVersions[] = "sync.invalidation_versions";
const char kSyncInvalidationVersions2[] = "sync.invalidation_versions2";

// Obsolete pref.
const char kSyncObsoleteKeystoreEncryptionBootstrapToken[] =
    "sync.keystore_encryption_bootstrap_token";

void UpdateInvalidationVersions(
    const std::map<ModelType, int64_t>& invalidation_versions,
    PrefService* pref_service) {
  base::Value::Dict invalidation_dictionary;
  for (const auto& [type, version] : invalidation_versions) {
    invalidation_dictionary.Set(
        base::NumberToString(GetSpecificsFieldNumberFromModelType(type)),
        base::NumberToString(version));
  }
  pref_service->SetDict(kSyncInvalidationVersions2,
                        std::move(invalidation_dictionary));
}

std::string GetLegacyModelTypeNameForInvalidationVersions(ModelType type) {
  switch (type) {
    case UNSPECIFIED:
      NOTREACHED();
      break;
    case BOOKMARKS:
      return "Bookmarks";
    case PREFERENCES:
      return "Preferences";
    case PASSWORDS:
      return "Passwords";
    case AUTOFILL_PROFILE:
      return "Autofill Profiles";
    case AUTOFILL:
      return "Autofill";
    case AUTOFILL_WALLET_DATA:
      return "Autofill Wallet";
    case AUTOFILL_WALLET_METADATA:
      return "Autofill Wallet Metadata";
    case AUTOFILL_WALLET_OFFER:
      return "Autofill Wallet Offer";
    case THEMES:
      return "Themes";
    case TYPED_URLS:
      return "Typed URLs";
    case EXTENSIONS:
      return "Extensions";
    case SEARCH_ENGINES:
      return "Search Engines";
    case SESSIONS:
      return "Sessions";
    case APPS:
      return "Apps";
    case APP_SETTINGS:
      return "App settings";
    case EXTENSION_SETTINGS:
      return "Extension settings";
    case HISTORY_DELETE_DIRECTIVES:
      return "History Delete Directives";
    case DICTIONARY:
      return "Dictionary";
    case DEVICE_INFO:
      return "Device Info";
    case PRIORITY_PREFERENCES:
      return "Priority Preferences";
    case SUPERVISED_USER_SETTINGS:
      return "Managed User Settings";
    case APP_LIST:
      return "App List";
    case ARC_PACKAGE:
      return "Arc Package";
    case PRINTERS:
      return "Printers";
    case READING_LIST:
      return "Reading List";
    case USER_EVENTS:
      return "User Events";
    case SECURITY_EVENTS:
      return "Security Events";
    case USER_CONSENTS:
      return "User Consents";
    case SEND_TAB_TO_SELF:
      return "Send Tab To Self";
    case PROXY_TABS:
      NOTREACHED();
      break;
    case NIGORI:
      return "Encryption Keys";
    case WEB_APPS:
      return "Web Apps";
    case WIFI_CONFIGURATIONS:
      return "Wifi Configurations";
    case WORKSPACE_DESK:
      return "Workspace Desk";
    case OS_PREFERENCES:
      return "OS Preferences";
    case OS_PRIORITY_PREFERENCES:
      return "OS Priority Preferences";
    case SHARING_MESSAGE:
      return "Sharing Message";
    default:
      // Note: There is no need to add new data types here. This code is only
      // used for a migration, so data types introduced after the migration
      // don't need to be handled.
      break;
  }
  return std::string();
}

}  // namespace

SyncTransportDataPrefs::SyncTransportDataPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {}

SyncTransportDataPrefs::~SyncTransportDataPrefs() = default;

// static
void SyncTransportDataPrefs::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kSyncGaiaId, std::string());
  registry->RegisterStringPref(kSyncCacheGuid, std::string());
  registry->RegisterStringPref(kSyncBirthday, std::string());
  registry->RegisterStringPref(kSyncBagOfChips, std::string());
  registry->RegisterTimePref(kSyncLastSyncedTime, base::Time());
  registry->RegisterTimePref(kSyncLastPollTime, base::Time());
  registry->RegisterTimeDeltaPref(kSyncPollIntervalSeconds, base::TimeDelta());
  registry->RegisterDictionaryPref(kDeprecatedSyncInvalidationVersions);
  registry->RegisterDictionaryPref(kSyncInvalidationVersions2);

  // Obsolete pref.
  registry->RegisterStringPref(kSyncObsoleteKeystoreEncryptionBootstrapToken,
                               std::string());
}

void SyncTransportDataPrefs::ClearAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pref_service_->ClearPref(kSyncLastSyncedTime);
  pref_service_->ClearPref(kSyncLastPollTime);
  pref_service_->ClearPref(kSyncPollIntervalSeconds);
  pref_service_->ClearPref(kSyncInvalidationVersions2);
  pref_service_->ClearPref(kDeprecatedSyncInvalidationVersions);
  pref_service_->ClearPref(kSyncGaiaId);
  pref_service_->ClearPref(kSyncCacheGuid);
  pref_service_->ClearPref(kSyncBirthday);
  pref_service_->ClearPref(kSyncBagOfChips);
}

base::Time SyncTransportDataPrefs::GetLastSyncedTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetTime(kSyncLastSyncedTime);
}

void SyncTransportDataPrefs::SetLastSyncedTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetTime(kSyncLastSyncedTime, time);
}

base::Time SyncTransportDataPrefs::GetLastPollTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetTime(kSyncLastPollTime);
}

void SyncTransportDataPrefs::SetLastPollTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetTime(kSyncLastPollTime, time);
}

base::TimeDelta SyncTransportDataPrefs::GetPollInterval() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeDelta poll_interval =
      pref_service_->GetTimeDelta(kSyncPollIntervalSeconds);
  // If the poll interval is unreasonably short, reset it. This will cause
  // callers to use a reasonable default value instead.
  // This fixes a past bug where stored pref values were accidentally
  // re-interpreted from "seconds" to "microseconds"; see crbug.com/1246850.
  if (poll_interval < base::Minutes(1)) {
    pref_service_->ClearPref(kSyncPollIntervalSeconds);
    return base::TimeDelta();
  }
  return poll_interval;
}

void SyncTransportDataPrefs::SetPollInterval(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetTimeDelta(kSyncPollIntervalSeconds, interval);
}

void SyncTransportDataPrefs::SetGaiaId(const std::string& gaia_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(kSyncGaiaId, gaia_id);
}

std::string SyncTransportDataPrefs::GetGaiaId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(kSyncGaiaId);
}

void SyncTransportDataPrefs::SetCacheGuid(const std::string& cache_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(kSyncCacheGuid, cache_guid);
}

std::string SyncTransportDataPrefs::GetCacheGuid() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(kSyncCacheGuid);
}

void SyncTransportDataPrefs::SetBirthday(const std::string& birthday) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(kSyncBirthday, birthday);
}

std::string SyncTransportDataPrefs::GetBirthday() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(kSyncBirthday);
}

void SyncTransportDataPrefs::SetBagOfChips(const std::string& bag_of_chips) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |bag_of_chips| contains a serialized proto which is not utf-8, hence we use
  // base64 encoding in prefs.
  std::string encoded;
  base::Base64Encode(bag_of_chips, &encoded);
  pref_service_->SetString(kSyncBagOfChips, encoded);
}

std::string SyncTransportDataPrefs::GetBagOfChips() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |kSyncBagOfChips| gets stored in base64 because it represents a serialized
  // proto which is not utf-8 encoding.
  const std::string encoded = pref_service_->GetString(kSyncBagOfChips);
  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  return decoded;
}

// static
void SyncTransportDataPrefs::MigrateInvalidationVersions(
    PrefService* pref_service) {
  const base::Value::Dict& invalidation_dictionary =
      pref_service->GetDict(kDeprecatedSyncInvalidationVersions);
  if (invalidation_dictionary.empty()) {
    // No data, or was already migrated. Nothing to do here.
    return;
  }

  // Read data from the deprecated pref.
  std::map<ModelType, int64_t> invalidation_versions;
  for (ModelType type : ProtocolTypes()) {
    std::string key = GetLegacyModelTypeNameForInvalidationVersions(type);
    // The key may be empty, e.g. for data types introduced after this
    // migration.
    if (key.empty())
      continue;
    const std::string* version_str = invalidation_dictionary.FindString(key);
    if (!version_str)
      continue;
    int64_t version = 0;
    if (!base::StringToInt64(*version_str, &version))
      continue;
    invalidation_versions[type] = version;
  }

  // Write to the new pref and clear the deprecated one.
  syncer::UpdateInvalidationVersions(invalidation_versions, pref_service);
  pref_service->ClearPref(kDeprecatedSyncInvalidationVersions);
}

std::map<ModelType, int64_t> SyncTransportDataPrefs::GetInvalidationVersions()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<ModelType, int64_t> invalidation_versions;
  const base::Value::Dict& invalidation_dictionary =
      pref_service_->GetDict(kSyncInvalidationVersions2);
  for (ModelType type : ProtocolTypes()) {
    std::string key =
        base::NumberToString(GetSpecificsFieldNumberFromModelType(type));
    const std::string* version_str = invalidation_dictionary.FindString(key);
    if (!version_str)
      continue;
    int64_t version = 0;
    if (!base::StringToInt64(*version_str, &version))
      continue;
    invalidation_versions[type] = version;
  }
  return invalidation_versions;
}

void SyncTransportDataPrefs::UpdateInvalidationVersions(
    const std::map<ModelType, int64_t>& invalidation_versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  syncer::UpdateInvalidationVersions(invalidation_versions, pref_service_);
}

void ClearObsoleteKeystoreBootstrapTokenPref(PrefService* pref_service) {
  pref_service->ClearPref(kSyncObsoleteKeystoreEncryptionBootstrapToken);
}

}  // namespace syncer

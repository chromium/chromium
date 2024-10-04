// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/glue/sync_transport_data_prefs.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/sync/base/account_pref_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"

namespace syncer {

namespace {

const char kSyncGaiaId[] = "sync.gaia_id";

const char kSyncTransportDataPerAccount[] = "sync.transport_data_per_account";

const char kSyncCacheGuid[] = "sync.cache_guid";
const char kSyncBirthday[] = "sync.birthday";
const char kSyncBagOfChips[] = "sync.bag_of_chips";

// 64-bit integer serialization of the base::Time when the last sync occurred.
const char kSyncLastSyncedTime[] = "sync.last_synced_time";

// 64-bit integer serialization of the base::Time of the last sync poll.
const char kSyncLastPollTime[] = "sync.last_poll_time";

// 64-bit integer serialization of base::TimeDelta storing poll intervals
// received by the server. For historic reasons, this is called
// "short_poll_interval", but it's not worth the hassle to rename it.
const char kSyncPollInterval[] = "sync.short_poll_interval";

}  // namespace

SyncTransportDataPrefs::SyncTransportDataPrefs(
    PrefService* pref_service,
    const signin::GaiaIdHash& gaia_id_hash)
    : pref_service_(pref_service), gaia_id_hash_(gaia_id_hash) {
  // If the account-keyed prefs aren't populated yet, copy over the values from
  // the legacy prefs.
  // TODO(crbug.com/360888481): Clean up this migration after 2025-05 or so.
  // As a sanity check, ensure that the Gaia IDs match.
  std::string old_gaia_id = pref_service_->GetString(kSyncGaiaId);
  if (signin::GaiaIdHash::FromGaiaId(old_gaia_id) == gaia_id_hash_ &&
      !pref_service_->HasPrefPath(kSyncTransportDataPerAccount)) {
    ScopedDictPrefUpdate update_account_dict(pref_service_,
                                             kSyncTransportDataPerAccount);
    base::Value::Dict* account_values =
        update_account_dict->EnsureDict(gaia_id_hash_.ToBase64());
    account_values->Set(kSyncCacheGuid,
                        pref_service_->GetValue(kSyncCacheGuid).Clone());
    account_values->Set(kSyncBirthday,
                        pref_service_->GetValue(kSyncBirthday).Clone());
    account_values->Set(kSyncBagOfChips,
                        pref_service_->GetValue(kSyncBagOfChips).Clone());
    account_values->Set(kSyncLastSyncedTime,
                        pref_service_->GetValue(kSyncLastSyncedTime).Clone());
    account_values->Set(kSyncLastPollTime,
                        pref_service_->GetValue(kSyncLastPollTime).Clone());
    account_values->Set(kSyncPollInterval,
                        pref_service_->GetValue(kSyncPollInterval).Clone());
  }
  // Whether values were migrated or not, clean up the legacy prefs now.
  pref_service_->ClearPref(kSyncCacheGuid);
  pref_service_->ClearPref(kSyncBirthday);
  pref_service_->ClearPref(kSyncBagOfChips);
  pref_service_->ClearPref(kSyncLastSyncedTime);
  pref_service_->ClearPref(kSyncLastPollTime);
  pref_service_->ClearPref(kSyncPollInterval);
}

SyncTransportDataPrefs::~SyncTransportDataPrefs() = default;

// static
void SyncTransportDataPrefs::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kSyncGaiaId, std::string());

  registry->RegisterDictionaryPref(kSyncTransportDataPerAccount);

  // TODO(crbug.com/360888481): Clean up the legacy non-account-keyed prefs
  // after 2025-05 or so.
  registry->RegisterStringPref(kSyncCacheGuid, std::string());
  registry->RegisterStringPref(kSyncBirthday, std::string());
  registry->RegisterStringPref(kSyncBagOfChips, std::string());
  registry->RegisterTimePref(kSyncLastSyncedTime, base::Time());
  registry->RegisterTimePref(kSyncLastPollTime, base::Time());
  registry->RegisterTimeDeltaPref(kSyncPollInterval, base::TimeDelta());
}

void SyncTransportDataPrefs::ClearForCurrentAccount() {
  ClearAccountKeyedPrefValue(pref_service_, kSyncTransportDataPerAccount,
                             gaia_id_hash_);
}

// static
void SyncTransportDataPrefs::KeepAccountSettingsPrefsOnlyForUsers(
    PrefService* pref_service,
    const std::vector<signin::GaiaIdHash>& available_gaia_ids) {
  KeepAccountKeyedPrefValuesOnlyForUsers(
      pref_service, kSyncTransportDataPerAccount, available_gaia_ids);
}

base::Time SyncTransportDataPrefs::GetLastSyncedTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value =
      GetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                                   gaia_id_hash_, kSyncLastSyncedTime);
  return base::ValueToTime(value).value_or(base::Time());
}

void SyncTransportDataPrefs::SetLastSyncedTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                               gaia_id_hash_, kSyncLastSyncedTime,
                               base::TimeToValue(time));
}

base::Time SyncTransportDataPrefs::GetLastPollTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value =
      GetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                                   gaia_id_hash_, kSyncLastPollTime);
  return base::ValueToTime(value).value_or(base::Time());
}

void SyncTransportDataPrefs::SetLastPollTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                               gaia_id_hash_, kSyncLastPollTime,
                               base::TimeToValue(time));
}

base::TimeDelta SyncTransportDataPrefs::GetPollInterval() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value =
      GetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                                   gaia_id_hash_, kSyncPollInterval);
  base::TimeDelta poll_interval =
      base::ValueToTimeDelta(value).value_or(base::TimeDelta());

  // If the poll interval is unreasonably short, reset it. This will cause
  // callers to use a reasonable default value instead.
  // This fixes a past bug where stored pref values were accidentally
  // re-interpreted from "seconds" to "microseconds"; see crbug.com/1246850.
  if (poll_interval < base::Minutes(1)) {
    pref_service_->ClearPref(kSyncPollInterval);
    return base::TimeDelta();
  }
  return poll_interval;
}

void SyncTransportDataPrefs::SetPollInterval(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                               gaia_id_hash_, kSyncPollInterval,
                               base::TimeDeltaToValue(interval));
}

void SyncTransportDataPrefs::SetCurrentSyncingGaiaId(
    const std::string& gaia_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetString(kSyncGaiaId, gaia_id);
}

std::string SyncTransportDataPrefs::GetCurrentSyncingGaiaId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetString(kSyncGaiaId);
}

void SyncTransportDataPrefs::ClearCurrentSyncingGaiaId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(kSyncGaiaId);
}

// static
bool SyncTransportDataPrefs::HasCurrentSyncingGaiaId(
    const PrefService* pref_service) {
  return !pref_service->GetString(kSyncGaiaId).empty();
}

// static
void SyncTransportDataPrefs::ClearCurrentSyncingGaiaId(
    PrefService* pref_service) {
  pref_service->ClearPref(kSyncGaiaId);
}

void SyncTransportDataPrefs::SetCacheGuid(const std::string& cache_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                               gaia_id_hash_, kSyncCacheGuid,
                               base::Value(cache_guid));
}

std::string SyncTransportDataPrefs::GetCacheGuid() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value =
      GetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                                   gaia_id_hash_, kSyncCacheGuid);
  return value && value->is_string() ? value->GetString() : std::string();
}

void SyncTransportDataPrefs::SetBirthday(const std::string& birthday) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                               gaia_id_hash_, kSyncBirthday,
                               base::Value(birthday));
}

std::string SyncTransportDataPrefs::GetBirthday() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* value =
      GetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                                   gaia_id_hash_, kSyncBirthday);
  return value && value->is_string() ? value->GetString() : std::string();
}

void SyncTransportDataPrefs::SetBagOfChips(const std::string& bag_of_chips) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |bag_of_chips| contains a serialized proto which is not utf-8, hence
  // we use base64 encoding in prefs.
  std::string encoded = base::Base64Encode(bag_of_chips);
  SetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                               gaia_id_hash_, kSyncBagOfChips,
                               base::Value(encoded));
}

std::string SyncTransportDataPrefs::GetBagOfChips() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string encoded;
  const base::Value* value =
      GetAccountKeyedPrefDictEntry(pref_service_, kSyncTransportDataPerAccount,
                                   gaia_id_hash_, kSyncBagOfChips);
  if (value && value->is_string()) {
    encoded = value->GetString();
  }

  // |kSyncBagOfChips| gets stored in base64 because it represents a serialized
  // proto which is not utf-8 encoding.
  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  return decoded;
}

}  // namespace syncer

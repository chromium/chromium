// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/glue/sync_transport_data_prefs.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"

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
}

void SyncTransportDataPrefs::ClearAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pref_service_->ClearPref(kSyncLastSyncedTime);
  pref_service_->ClearPref(kSyncLastPollTime);
  pref_service_->ClearPref(kSyncPollIntervalSeconds);
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

}  // namespace syncer

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_prefs.h"

#include <algorithm>
#include <utility>

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace syncer {
namespace {

// Preference name for storing recently used cache GUIDs and their timestamps
// in days since Windows epoch. Most recent first.
const char kDeviceInfoRecentGUIDsWithTimestamps[] =
    "sync.local_device_guids_with_timestamp";

// Keys used in the dictionaries stored in prefs.
const char kCacheGuidKey[] = "cache_guid";
const char kTimestampKey[] = "timestamp";

// The max time a local device's cached GUIDs will be stored.
constexpr base::TimeDelta kMaxTimeDeltaLocalCacheGuidsStored = base::Days(10);

// The max number of local device most recent cached GUIDs that will be stored
// in preferences.
constexpr int kMaxLocalCacheGuidsStored = 30;

// Returns true iff |dict| is a dictionary with a cache GUID that is equal to
// |cache_guid|.
bool MatchesGuidInDictionary(const base::Value& dict,
                             const std::string& cache_guid) {
  if (!dict.is_dict()) {
    return false;
  }
  const std::string* v_cache_guid = dict.FindStringKey(kCacheGuidKey);
  return v_cache_guid && *v_cache_guid == cache_guid;
}

}  // namespace

// static
void DeviceInfoPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kDeviceInfoRecentGUIDsWithTimestamps);
}

DeviceInfoPrefs::DeviceInfoPrefs(PrefService* pref_service,
                                 const base::Clock* clock)
    : pref_service_(pref_service), clock_(clock) {
  DCHECK(pref_service_);
  DCHECK(clock_);
}

DeviceInfoPrefs::~DeviceInfoPrefs() = default;

bool DeviceInfoPrefs::IsRecentLocalCacheGuid(
    const std::string& cache_guid) const {
  base::Value::ConstListView recent_local_cache_guids =
      pref_service_->GetList(kDeviceInfoRecentGUIDsWithTimestamps)
          ->GetListDeprecated();

  for (const auto& v : recent_local_cache_guids) {
    if (MatchesGuidInDictionary(v, cache_guid)) {
      return true;
    }
  }

  return false;
}

void DeviceInfoPrefs::AddLocalCacheGuid(const std::string& cache_guid) {
  ListPrefUpdate update_cache_guids(pref_service_,
                                    kDeviceInfoRecentGUIDsWithTimestamps);

  for (auto it = update_cache_guids->GetListDeprecated().begin();
       it != update_cache_guids->GetListDeprecated().end(); it++) {
    if (MatchesGuidInDictionary(*it, cache_guid)) {
      // Remove it from the list, to be reinserted below, in the first
      // position.
      update_cache_guids->EraseListIter(it);
      break;
    }
  }

  base::Value new_entry(base::Value::Type::DICTIONARY);
  new_entry.SetKey(kCacheGuidKey, base::Value(cache_guid));
  new_entry.SetKey(
      kTimestampKey,
      base::Value(clock_->Now().ToDeltaSinceWindowsEpoch().InDays()));

  update_cache_guids->Insert(update_cache_guids->GetListDeprecated().begin(),
                             std::move(new_entry));

  while (update_cache_guids->GetListDeprecated().size() >
         kMaxLocalCacheGuidsStored) {
    update_cache_guids->EraseListIter(
        update_cache_guids->GetListDeprecated().end() - 1);
  }
}

void DeviceInfoPrefs::GarbageCollectExpiredCacheGuids() {
  ListPrefUpdate update_cache_guids(pref_service_,
                                    kDeviceInfoRecentGUIDsWithTimestamps);
  update_cache_guids->EraseListValueIf([this](const auto& dict) {
    // Avoid crashes if the preference contains corrupt entries that are not
    // dictionaries, and meanwhile clean up these corrupt entries.
    if (!dict.is_dict()) {
      return true;
    }

    absl::optional<int> days_since_epoch = dict.FindIntKey(kTimestampKey);

    // Avoid crashes if the dictionary contains no timestamp and meanwhile clean
    // up these corrupt entries.
    if (!days_since_epoch.has_value()) {
      return true;
    }

    const base::Time creation_time =
        base::Time::FromDeltaSinceWindowsEpoch(base::Days(*days_since_epoch));
    return creation_time < clock_->Now() - kMaxTimeDeltaLocalCacheGuidsStored;
  });
}

}  // namespace syncer

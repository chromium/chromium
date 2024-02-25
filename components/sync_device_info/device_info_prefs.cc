// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_prefs.h"

#include <algorithm>
#include <utility>

#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/trace_event/trace_event.h"
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
  const std::string* v_cache_guid = dict.GetDict().FindString(kCacheGuidKey);
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
  TRACE_EVENT0("sync", "DeviceInfoPrefs::IsRecentLocalCacheGuid");
  const base::Value::List& recent_local_cache_guids =
      pref_service_->GetList(kDeviceInfoRecentGUIDsWithTimestamps);

  for (const auto& v : recent_local_cache_guids) {
    if (MatchesGuidInDictionary(v, cache_guid)) {
      return true;
    }
  }

  return false;
}

void DeviceInfoPrefs::AddLocalCacheGuid(const std::string& cache_guid) {
  TRACE_EVENT0("sync", "DeviceInfoPrefs::AddLocalCacheGuid");
  ScopedListPrefUpdate update_cache_guids(pref_service_,
                                          kDeviceInfoRecentGUIDsWithTimestamps);
  base::Value::List& update_list = update_cache_guids.Get();

  for (auto it = update_list.begin(); it != update_list.end(); it++) {
    if (MatchesGuidInDictionary(*it, cache_guid)) {
      // Remove it from the list, to be reinserted below, in the first
      // position.
      update_list.erase(it);
      break;
    }
  }

  base::Value::Dict new_entry;
  new_entry.Set(kCacheGuidKey, cache_guid);
  new_entry.Set(kTimestampKey,
                clock_->Now().ToDeltaSinceWindowsEpoch().InDays());

  update_list.Insert(update_list.begin(), base::Value(std::move(new_entry)));

  if (update_list.size() > kMaxLocalCacheGuidsStored) {
    update_list.erase(update_list.begin() + kMaxLocalCacheGuidsStored,
                      update_list.end());
  }
}

void DeviceInfoPrefs::GarbageCollectExpiredCacheGuids() {
  ScopedListPrefUpdate update_cache_guids(pref_service_,
                                          kDeviceInfoRecentGUIDsWithTimestamps);
  update_cache_guids->EraseIf([this](const auto& dict) {
    // Avoid crashes if the preference contains corrupt entries that are not
    // dictionaries, and meanwhile clean up these corrupt entries.
    if (!dict.is_dict()) {
      return true;
    }

    std::optional<int> days_since_epoch = dict.GetDict().FindInt(kTimestampKey);

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

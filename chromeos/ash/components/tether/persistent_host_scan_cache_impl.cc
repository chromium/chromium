// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/persistent_host_scan_cache_impl.h"

#include <memory>
#include <unordered_set>

#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/ash/components/tether/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace tether {

namespace {

constexpr char kTetherNetworkGuidKey[] = "tether_network_guid";
constexpr char kDeviceNameKey[] = "device_name";
constexpr char kCarrierKey[] = "carrier";
constexpr char kBatteryPercentageKey[] = "battery_percentage";
constexpr char kSignalStrengthKey[] = "signal_strength";
constexpr char kSetupRequiredKey[] = "setup_required";

base::Value::Dict HostScanCacheEntryToDictionary(
    const HostScanCacheEntry& entry) {
  base::Value::Dict dictionary;

  dictionary.Set(kTetherNetworkGuidKey, entry.tether_network_guid);
  dictionary.Set(kDeviceNameKey, entry.device_name);
  dictionary.Set(kCarrierKey, entry.carrier);
  dictionary.Set(kBatteryPercentageKey, entry.battery_percentage);
  dictionary.Set(kSignalStrengthKey, entry.signal_strength);
  dictionary.Set(kSetupRequiredKey, entry.setup_required);

  return dictionary;
}

std::unique_ptr<HostScanCacheEntry> DictionaryToHostScanCacheEntry(
    const base::Value::Dict& dictionary) {
  HostScanCacheEntry::Builder builder;

  const std::string* tether_network_guid =
      dictionary.FindString(kTetherNetworkGuidKey);
  if (!tether_network_guid || tether_network_guid->empty())
    return nullptr;
  builder.SetTetherNetworkGuid(*tether_network_guid);

  const std::string* device_name = dictionary.FindString(kDeviceNameKey);
  if (!device_name)
    return nullptr;
  builder.SetDeviceName(*device_name);

  const std::string* carrier = dictionary.FindString(kCarrierKey);
  if (!carrier)
    return nullptr;
  builder.SetCarrier(*carrier);

  std::optional<int> battery_percentage =
      dictionary.FindInt(kBatteryPercentageKey);
  if (!battery_percentage || *battery_percentage < 0 ||
      *battery_percentage > 100) {
    return nullptr;
  }
  builder.SetBatteryPercentage(*battery_percentage);

  std::optional<int> signal_strength = dictionary.FindInt(kSignalStrengthKey);
  if (!signal_strength || *signal_strength < 0 || *signal_strength > 100) {
    return nullptr;
  }
  builder.SetSignalStrength(*signal_strength);

  std::optional<bool> setup_required = dictionary.FindBool(kSetupRequiredKey);
  if (!setup_required)
    return nullptr;

  builder.SetSetupRequired(*setup_required);

  return builder.Build();
}

}  // namespace

// static
void PersistentHostScanCacheImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kHostScanCache);
}

PersistentHostScanCacheImpl::PersistentHostScanCacheImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

PersistentHostScanCacheImpl::~PersistentHostScanCacheImpl() = default;

std::unordered_map<std::string, HostScanCacheEntry>
PersistentHostScanCacheImpl::GetStoredCacheEntries() {
  const base::Value::List& cache_entry_list =
      pref_service_->GetList(prefs::kHostScanCache);

  std::unordered_map<std::string, HostScanCacheEntry> entries;
  std::unordered_set<std::string> ids_processed_so_far;
  for (auto& cache_entry_value : cache_entry_list) {
    if (!cache_entry_value.is_dict()) {
      // All prefs stored in the ListValue should be valid DictionaryValues.
      NOTREACHED_IN_MIGRATION();
    }

    std::unique_ptr<HostScanCacheEntry> entry =
        DictionaryToHostScanCacheEntry(cache_entry_value.GetDict());
    DCHECK(entry);

    std::string tether_network_guid = entry->tether_network_guid;
    DCHECK(!tether_network_guid.empty());

    // There should never be duplicate entries stored for one Tether network
    // GUID.
    DCHECK(ids_processed_so_far.find(tether_network_guid) ==
           ids_processed_so_far.end());
    ids_processed_so_far.insert(tether_network_guid);

    entries.emplace(tether_network_guid, *entry);
  }

  return entries;
}

void PersistentHostScanCacheImpl::SetHostScanResult(
    const HostScanCacheEntry& entry) {
  std::unordered_map<std::string, HostScanCacheEntry> entries =
      GetStoredCacheEntries();

  // Erase any existing scan result for this GUID (if none currently exists,
  // this is a no-op).
  entries.erase(entry.tether_network_guid);

  // Add the entry supplied.
  entries.emplace(entry.tether_network_guid, entry);

  StoreCacheEntriesToPrefs(entries);
}

bool PersistentHostScanCacheImpl::RemoveHostScanResultImpl(
    const std::string& tether_network_guid) {
  std::unordered_map<std::string, HostScanCacheEntry> entries =
      GetStoredCacheEntries();

  bool result_was_removed = entries.erase(tether_network_guid);

  // Only store the updated entries if a scan result was actually removed.
  // Otherwise, nothing has changed and there is no reason to re-write the same
  // data.
  if (result_was_removed)
    StoreCacheEntriesToPrefs(entries);

  return result_was_removed;
}

bool PersistentHostScanCacheImpl::ExistsInCache(
    const std::string& tether_network_guid) {
  std::unordered_map<std::string, HostScanCacheEntry> entries =
      GetStoredCacheEntries();
  return entries.find(tether_network_guid) != entries.end();
}

std::unordered_set<std::string>
PersistentHostScanCacheImpl::GetTetherGuidsInCache() {
  std::unordered_set<std::string> tether_guids;
  for (const auto& entry : GetStoredCacheEntries())
    tether_guids.insert(entry.first);
  return tether_guids;
}

bool PersistentHostScanCacheImpl::DoesHostRequireSetup(
    const std::string& tether_network_guid) {
  std::unordered_map<std::string, HostScanCacheEntry> entries =
      GetStoredCacheEntries();

  auto it = entries.find(tether_network_guid);
  DCHECK(it != entries.end());

  return it->second.setup_required;
}

void PersistentHostScanCacheImpl::StoreCacheEntriesToPrefs(
    const std::unordered_map<std::string, HostScanCacheEntry>& entries) {
  base::Value::List entries_list;

  for (const auto& it : entries) {
    entries_list.Append(base::Value(HostScanCacheEntryToDictionary(it.second)));
  }

  pref_service_->SetList(prefs::kHostScanCache, std::move(entries_list));
}

}  // namespace tether

}  // namespace ash

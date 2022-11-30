// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_host_scan_cache.h"

namespace ash {

namespace tether {

FakeHostScanCache::FakeHostScanCache() : HostScanCache() {}

FakeHostScanCache::~FakeHostScanCache() = default;

const HostScanCacheEntry* FakeHostScanCache::GetCacheEntry(
    const std::string& tether_network_guid) {
  auto it = cache_.find(tether_network_guid);
  if (it == cache_.end())
    return nullptr;

  return &it->second;
}

void FakeHostScanCache::SetHostScanResult(const HostScanCacheEntry& entry) {
  // Erase any existing entry with the same GUID if it exists (if nothing
  // currently exists with that GUID, this is a no-op).
  cache_.erase(entry.tether_network_guid);

  // Add the new entry.
  cache_.emplace(entry.tether_network_guid, entry);
}

bool FakeHostScanCache::RemoveHostScanResultImpl(
    const std::string& tether_network_guid) {
  return cache_.erase(tether_network_guid) > 0;
}

std::unordered_set<std::string> FakeHostScanCache::GetTetherGuidsInCache() {
  std::unordered_set<std::string> tether_guids;
  for (const auto& entry : cache_)
    tether_guids.insert(entry.first);
  return tether_guids;
}

bool FakeHostScanCache::ExistsInCache(const std::string& tether_network_guid) {
  return GetCacheEntry(tether_network_guid) != nullptr;
}

bool FakeHostScanCache::DoesHostRequireSetup(
    const std::string& tether_network_guid) {
  auto it = cache_.find(tether_network_guid);
  if (it != cache_.end())
    return it->second.setup_required;

  return false;
}

}  // namespace tether

}  // namespace ash

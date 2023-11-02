// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_HOST_SCAN_CACHE_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_HOST_SCAN_CACHE_H_

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "chromeos/ash/components/tether/host_scan_cache.h"

namespace ash {

namespace tether {

// Test double for HostScanCache which stores cache results in memory.
class FakeHostScanCache : virtual public HostScanCache {
 public:
  FakeHostScanCache();

  FakeHostScanCache(const FakeHostScanCache&) = delete;
  FakeHostScanCache& operator=(const FakeHostScanCache&) = delete;

  ~FakeHostScanCache() override;

  // Getters for contents of the cache.
  const HostScanCacheEntry* GetCacheEntry(
      const std::string& tether_network_guid);
  size_t size() { return cache_.size(); }
  bool empty() { return cache_.empty(); }
  const std::unordered_map<std::string, HostScanCacheEntry> cache() {
    return cache_;
  }

  // HostScanCache:
  void SetHostScanResult(const HostScanCacheEntry& entry) override;
  bool ExistsInCache(const std::string& tether_network_guid) override;
  std::unordered_set<std::string> GetTetherGuidsInCache() override;
  bool DoesHostRequireSetup(const std::string& tether_network_guid) override;

 protected:
  bool RemoveHostScanResultImpl(
      const std::string& tether_network_guid) override;

 private:
  std::unordered_map<std::string, HostScanCacheEntry> cache_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_HOST_SCAN_CACHE_H_

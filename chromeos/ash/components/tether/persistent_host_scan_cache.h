// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_H_

#include "chromeos/ash/components/tether/host_scan_cache.h"

namespace ash {

namespace tether {

// HostScanCache implementation which stores scan results in persistent user
// prefs.
class PersistentHostScanCache : virtual public HostScanCache {
 public:
  PersistentHostScanCache() {}

  PersistentHostScanCache(const PersistentHostScanCache&) = delete;
  PersistentHostScanCache& operator=(const PersistentHostScanCache&) = delete;

  ~PersistentHostScanCache() override {}

  // Returns the cache entries that are currently stored in user prefs as a map
  // from Tether network GUID to entry.
  virtual std::unordered_map<std::string, HostScanCacheEntry>
  GetStoredCacheEntries() = 0;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_H_

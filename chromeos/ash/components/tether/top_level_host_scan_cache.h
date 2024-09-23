// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TOP_LEVEL_HOST_SCAN_CACHE_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TOP_LEVEL_HOST_SCAN_CACHE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/tether/host_scan_cache.h"

namespace ash::timer_factory {
class TimerFactory;
}  // namespace ash::timer_factory

namespace ash {

namespace tether {

class ActiveHost;
class PersistentHostScanCache;

// HostScanCache implementation which interfaces with the network stack as well
// as storing scanned device properties persistently and recovering stored
// properties after a browser crash. When SetHostScanResult() is called,
// TopLevelHostScanCache starts a timer which automatically removes scan results
// after |kNumMinutesBeforeCacheEntryExpires| minutes.
class TopLevelHostScanCache : public HostScanCache {
 public:
  // The number of minutes that a cache entry is considered to be valid before
  // it is removed from the cache. Very old host scan results are removed from
  // the cache because the results contain properties such as battery percentage
  // and signal strength which are ephemeral in nature. However, this timeout
  // value is chosen to be rather long because we assume that most users usually
  // do not move their Chrome OS devices physically away from their potential
  // tether host devices while in use. Note that when network settings UI is
  // opened, a new scan will be triggered, and any devices currently in the
  // cache which are not discovered during the scan are removed.
  static constexpr int kNumMinutesBeforeCacheEntryExpires = 120;

  TopLevelHostScanCache(
      std::unique_ptr<ash::timer_factory::TimerFactory> timer_factory,
      ActiveHost* active_host,
      HostScanCache* network_host_scan_cache,
      PersistentHostScanCache* persistent_host_scan_cache);

  TopLevelHostScanCache(const TopLevelHostScanCache&) = delete;
  TopLevelHostScanCache& operator=(const TopLevelHostScanCache&) = delete;

  ~TopLevelHostScanCache() override;

  // HostScanCache:
  void SetHostScanResult(const HostScanCacheEntry& entry) override;
  bool ExistsInCache(const std::string& tether_network_guid) override;
  std::unordered_set<std::string> GetTetherGuidsInCache() override;
  bool DoesHostRequireSetup(const std::string& tether_network_guid) override;

 protected:
  bool RemoveHostScanResultImpl(
      const std::string& tether_network_guid) override;

 private:
  friend class TopLevelHostScanCacheTest;

  void InitializeFromPersistentCache();
  void StartTimer(const std::string& tether_network_guid);
  void OnTimerFired(const std::string& tether_network_guid);

  std::unique_ptr<ash::timer_factory::TimerFactory> timer_factory_;
  raw_ptr<ActiveHost> active_host_;
  raw_ptr<HostScanCache> network_host_scan_cache_;
  raw_ptr<PersistentHostScanCache> persistent_host_scan_cache_;

  bool is_initializing_ = false;
  bool is_shutting_down_ = false;

  // Maps from the Tether network GUID to a Timer object. While a scan result is
  // active in the cache, the corresponding Timer object starts running; if the
  // timer fires, the result is removed (unless it corresponds to the active
  // host).
  std::unordered_map<std::string, std::unique_ptr<base::OneShotTimer>>
      tether_guid_to_timer_map_;
  base::WeakPtrFactory<TopLevelHostScanCache> weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TOP_LEVEL_HOST_SCAN_CACHE_H_

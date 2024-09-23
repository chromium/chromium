// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/top_level_host_scan_cache.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/tether/active_host.h"
#include "chromeos/ash/components/tether/persistent_host_scan_cache.h"
#include "chromeos/ash/components/timer_factory/timer_factory.h"

namespace ash {

namespace tether {

TopLevelHostScanCache::TopLevelHostScanCache(
    std::unique_ptr<ash::timer_factory::TimerFactory> timer_factory,
    ActiveHost* active_host,
    HostScanCache* network_host_scan_cache,
    PersistentHostScanCache* persistent_host_scan_cache)
    : timer_factory_(std::move(timer_factory)),
      active_host_(active_host),
      network_host_scan_cache_(network_host_scan_cache),
      persistent_host_scan_cache_(persistent_host_scan_cache) {
  InitializeFromPersistentCache();
}

TopLevelHostScanCache::~TopLevelHostScanCache() {
  DCHECK(ActiveHost::ActiveHostStatus::DISCONNECTED ==
         active_host_->GetActiveHostStatus());
  is_shutting_down_ = true;
  for (const auto& tether_guid : GetTetherGuidsInCache())
    RemoveHostScanResult(tether_guid);
}

void TopLevelHostScanCache::SetHostScanResult(const HostScanCacheEntry& entry) {
  auto found_iter = tether_guid_to_timer_map_.find(entry.tether_network_guid);
  if (found_iter == tether_guid_to_timer_map_.end()) {
    // Only check whether this entry exists in the cache after intialization
    // completes; otherwise, this will cause an error when the persistent cache
    // has entries that the other caches do not have.
    DCHECK(is_initializing_ || !ExistsInCache(entry.tether_network_guid));

    // If no Timer exists in the map, add one.
    tether_guid_to_timer_map_.emplace(entry.tether_network_guid,
                                      timer_factory_->CreateOneShotTimer());
  } else {
    DCHECK(ExistsInCache(entry.tether_network_guid));

    // If a timer was already running for this entry, stop it. It is started
    // again in the StartTimer() call below since the entry now has fresh data.
    found_iter->second->Stop();
  }

  // Set the result in the sub-caches.
  network_host_scan_cache_->SetHostScanResult(entry);
  persistent_host_scan_cache_->SetHostScanResult(entry);

  StartTimer(entry.tether_network_guid);
}

bool TopLevelHostScanCache::RemoveHostScanResultImpl(
    const std::string& tether_network_guid) {
  DCHECK(!tether_network_guid.empty());

  if (active_host_->GetTetherNetworkGuid() == tether_network_guid) {
    DCHECK(ExistsInCache(tether_network_guid));
    PA_LOG(VERBOSE) << "RemoveHostScanResult() called for Tether network with "
                    << "GUID " << tether_network_guid << ", but the "
                    << "corresponding device is the active host. Not removing "
                    << "this scan result from the cache.";
    return false;
  }

  if (!ExistsInCache(tether_network_guid)) {
    PA_LOG(ERROR) << "Attempted to remove a host scan result which does not "
                  << "exist in the cache. GUID: " << tether_network_guid;
    return false;
  }

  bool removed_from_network =
      network_host_scan_cache_->RemoveHostScanResult(tether_network_guid);
  bool removed_from_persistent =
      persistent_host_scan_cache_->RemoveHostScanResult(tether_network_guid);
  bool removed_from_timer_map =
      tether_guid_to_timer_map_.erase(tether_network_guid) == 1u;

  // The caches are expected to remain in sync, so it should not be possible
  // for one of them to be removed successfully while the other one fails.
  DCHECK(removed_from_network && removed_from_persistent &&
         removed_from_timer_map);

  PA_LOG(VERBOSE) << "Removed cache entry with GUID \"" << tether_network_guid
                  << "\".";

  // We already DCHECK()ed above that this evaluates to true, but we return the
  // AND'ed value here because without this, release builds (without DCHECK())
  // will produce a compiler warning of unused variables.
  return removed_from_network && removed_from_persistent &&
         removed_from_timer_map;
}

std::unordered_set<std::string> TopLevelHostScanCache::GetTetherGuidsInCache() {
  std::unordered_set<std::string> tether_guids;
  for (const auto& entry : tether_guid_to_timer_map_)
    tether_guids.insert(entry.first);

  CHECK(tether_guids == persistent_host_scan_cache_->GetTetherGuidsInCache());
  // It is expected that `network_host_scan_cache` is empty during shutdown
  // (but `tether_guid_to_timer_map_` may still contain recently seen hosts).
  if (!is_shutting_down_) {
    CHECK(tether_guids == network_host_scan_cache_->GetTetherGuidsInCache());
  }

  return tether_guids;
}

bool TopLevelHostScanCache::ExistsInCache(
    const std::string& tether_network_guid) {
  bool exists_in_network_cache =
      network_host_scan_cache_->ExistsInCache(tether_network_guid);
  bool exists_in_persistent_cache =
      persistent_host_scan_cache_->ExistsInCache(tether_network_guid);
  bool exists_in_timer_map =
      base::Contains(tether_guid_to_timer_map_, tether_network_guid);

  // The caches are expected to remain in sync.
  DCHECK(exists_in_network_cache == exists_in_persistent_cache &&
         exists_in_persistent_cache == exists_in_timer_map);

  // We already DCHECK()ed above that these are equal, but we return the AND'ed
  // value here because without this, release builds (without DCHECK())
  // will produce a compiler warning of unused variables.
  return exists_in_network_cache && exists_in_persistent_cache &&
         exists_in_timer_map;
}

bool TopLevelHostScanCache::DoesHostRequireSetup(
    const std::string& tether_network_guid) {
  // |network_host_scan_cache_| does not keep track of this value since the
  // networking stack does not store it internally. Instead, query
  // |persistent_host_scan_cache_|.
  return persistent_host_scan_cache_->DoesHostRequireSetup(tether_network_guid);
}

void TopLevelHostScanCache::InitializeFromPersistentCache() {
  is_initializing_ = true;

  // If a crash occurs, Tether networks which were previously present will no
  // longer be available since they are only stored within NetworkStateHandler
  // and not within Shill. Thus, utilize |persistent_host_scan_cache_| to fetch
  // metadata about all Tether networks which were present before the crash and
  // restore |network_host_scan_cache_|.
  std::unordered_map<std::string, HostScanCacheEntry> persisted_entries =
      persistent_host_scan_cache_->GetStoredCacheEntries();
  for (const auto& it : persisted_entries) {
    SetHostScanResult(it.second);
  }

  is_initializing_ = false;
}

void TopLevelHostScanCache::StartTimer(const std::string& tether_network_guid) {
  auto found_iter = tether_guid_to_timer_map_.find(tether_network_guid);
  DCHECK(found_iter != tether_guid_to_timer_map_.end());
  DCHECK(!found_iter->second->IsRunning());

  PA_LOG(VERBOSE)
      << "Starting host scan cache timer for Tether network with GUID "
      << "\"" << tether_network_guid << "\". Will fire in "
      << kNumMinutesBeforeCacheEntryExpires << " minutes.";

  found_iter->second->Start(
      FROM_HERE, base::Minutes(kNumMinutesBeforeCacheEntryExpires),
      base::BindOnce(&TopLevelHostScanCache::OnTimerFired,
                     weak_ptr_factory_.GetWeakPtr(), tether_network_guid));
}

void TopLevelHostScanCache::OnTimerFired(
    const std::string& tether_network_guid) {
  if (active_host_->GetTetherNetworkGuid() == tether_network_guid) {
    // Log as a warning. This situation should be uncommon in practice since
    // KeepAliveScheduler should schedule a new keep-alive status update every
    // 4 minutes.
    PA_LOG(WARNING) << "Timer fired for Tether network GUID \""
                    << tether_network_guid << "\", but the corresponding "
                    << "device is the active host. Restarting timer.";

    // If the Timer which fired corresponds to the active host, do not remove
    // the cache entry. The active host must always remain in the cache so that
    // the UI can reflect that it is the connecting/connected network. In this
    // case, just restart the timer.
    StartTimer(tether_network_guid);
    return;
  }

  PA_LOG(VERBOSE) << "Timer fired for Tether network GUID "
                  << tether_network_guid << ". Removing stale scan result.";
  RemoveHostScanResult(tether_network_guid);
}

}  // namespace tether

}  // namespace ash

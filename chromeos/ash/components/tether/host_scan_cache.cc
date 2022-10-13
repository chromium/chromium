// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_scan_cache.h"

namespace ash {

namespace tether {

HostScanCache::HostScanCache() = default;
HostScanCache::~HostScanCache() = default;

void HostScanCache::AddObserver(HostScanCache::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HostScanCache::RemoveObserver(HostScanCache::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool HostScanCache::RemoveHostScanResult(
    const std::string& tether_network_guid) {
  if (!RemoveHostScanResultImpl(tether_network_guid))
    return false;

  if (GetTetherGuidsInCache().empty()) {
    for (auto& observer : observer_list_)
      observer.OnCacheBecameEmpty();
  }

  return true;
}

}  // namespace tether

}  // namespace ash

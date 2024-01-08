// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_NETWORK_HOST_SCAN_CACHE_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_NETWORK_HOST_SCAN_CACHE_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/tether/host_scan_cache.h"
#include "chromeos/ash/components/tether/tether_host_response_recorder.h"

namespace ash {

class NetworkStateHandler;

namespace tether {

class DeviceIdTetherNetworkGuidMap;
class TetherHostResponseRecorder;

// HostScanCache implementation which stores scan results in the networking
// stack.
class NetworkHostScanCache : public HostScanCache,
                             public TetherHostResponseRecorder::Observer {
 public:
  NetworkHostScanCache(
      NetworkStateHandler* network_state_handler,
      TetherHostResponseRecorder* tether_host_response_recorder,
      DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map);

  NetworkHostScanCache(const NetworkHostScanCache&) = delete;
  NetworkHostScanCache& operator=(const NetworkHostScanCache&) = delete;

  ~NetworkHostScanCache() override;

  // HostScanCache:
  void SetHostScanResult(const HostScanCacheEntry& entry) override;
  bool ExistsInCache(const std::string& tether_network_guid) override;
  std::unordered_set<std::string> GetTetherGuidsInCache() override;
  bool DoesHostRequireSetup(const std::string& tether_network_guid) override;

  // TetherHostResponseRecorder::Observer:
  void OnPreviouslyConnectedHostIdsChanged() override;

 protected:
  bool RemoveHostScanResultImpl(
      const std::string& tether_network_guid) override;

 private:
  friend class NetworkHostScanCacheTest;

  bool HasConnectedToHost(const std::string& tether_network_guid);

  raw_ptr<NetworkStateHandler> network_state_handler_;
  raw_ptr<TetherHostResponseRecorder> tether_host_response_recorder_;
  raw_ptr<DeviceIdTetherNetworkGuidMap> device_id_tether_network_guid_map_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_NETWORK_HOST_SCAN_CACHE_H_

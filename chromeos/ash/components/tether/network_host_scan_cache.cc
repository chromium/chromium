// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/network_host_scan_cache.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/tether/device_id_tether_network_guid_map.h"
#include "chromeos/ash/components/tether/tether_host_response_recorder.h"

namespace ash {

namespace tether {

NetworkHostScanCache::NetworkHostScanCache(
    NetworkStateHandler* network_state_handler,
    TetherHostResponseRecorder* tether_host_response_recorder,
    DeviceIdTetherNetworkGuidMap* device_id_tether_network_guid_map)
    : network_state_handler_(network_state_handler),
      tether_host_response_recorder_(tether_host_response_recorder),
      device_id_tether_network_guid_map_(device_id_tether_network_guid_map) {
  tether_host_response_recorder_->AddObserver(this);
}

NetworkHostScanCache::~NetworkHostScanCache() {
  tether_host_response_recorder_->RemoveObserver(this);
}

void NetworkHostScanCache::SetHostScanResult(const HostScanCacheEntry& entry) {
  if (!ExistsInCache(entry.tether_network_guid)) {
    network_state_handler_->AddTetherNetworkState(
        entry.tether_network_guid, entry.device_name, entry.carrier,
        entry.battery_percentage, entry.signal_strength,
        HasConnectedToHost(entry.tether_network_guid));

    PA_LOG(VERBOSE) << "Added scan result for Tether network with GUID "
                    << entry.tether_network_guid << ". "
                    << "Device name: " << entry.device_name << ", "
                    << "carrier: " << entry.carrier << ", "
                    << "battery percentage: " << entry.battery_percentage
                    << ", "
                    << "signal strength: " << entry.signal_strength;
  } else {
    network_state_handler_->UpdateTetherNetworkProperties(
        entry.tether_network_guid, entry.carrier, entry.battery_percentage,
        entry.signal_strength);

    PA_LOG(VERBOSE) << "Updated scan result for Tether network with GUID "
                    << entry.tether_network_guid << ". "
                    << "New carrier: " << entry.carrier << ", "
                    << "new battery percentage: " << entry.battery_percentage
                    << ", new signal strength: " << entry.signal_strength;
  }
}

bool NetworkHostScanCache::RemoveHostScanResultImpl(
    const std::string& tether_network_guid) {
  DCHECK(!tether_network_guid.empty());
  return network_state_handler_->RemoveTetherNetworkState(tether_network_guid);
}

bool NetworkHostScanCache::ExistsInCache(
    const std::string& tether_network_guid) {
  return network_state_handler_->GetNetworkStateFromGuid(tether_network_guid) !=
         nullptr;
}

std::unordered_set<std::string> NetworkHostScanCache::GetTetherGuidsInCache() {
  NetworkStateHandler::NetworkStateList tether_network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Tether(), &tether_network_list);

  std::unordered_set<std::string> tether_guids;
  for (auto* const network : tether_network_list)
    tether_guids.insert(network->guid());
  return tether_guids;
}

bool NetworkHostScanCache::DoesHostRequireSetup(
    const std::string& tether_network_guid) {
  // Not implemented by NetworkHostScanCache since this boolean is not stored in
  // the network stack.
  NOTIMPLEMENTED();
  return false;
}

void NetworkHostScanCache::OnPreviouslyConnectedHostIdsChanged() {
  std::vector<std::string> previously_connected_host_ids =
      tether_host_response_recorder_->GetPreviouslyConnectedHostIds();
  for (auto& tether_network_guid : previously_connected_host_ids) {
    if (!ExistsInCache(tether_network_guid)) {
      // If the network with GUID |tether_network_guid| is not present in
      // |network_state_handler_|, skip it.
      continue;
    }

    // Set that this network has connected to a host. Note that this function is
    // a no-op if it is called on a network which already has its
    // HasConnectedToHost property set to true.
    bool update_successful =
        network_state_handler_->SetTetherNetworkHasConnectedToHost(
            tether_network_guid);

    if (update_successful) {
      PA_LOG(VERBOSE) << "Successfully set the HasConnectedToHost property of "
                      << "the Tether network with GUID " << tether_network_guid
                      << " to true.";
    }
  }
}

bool NetworkHostScanCache::HasConnectedToHost(
    const std::string& tether_network_guid) {
  std::string device_id =
      device_id_tether_network_guid_map_->GetDeviceIdForTetherNetworkGuid(
          tether_network_guid);
  std::vector<std::string> connected_device_ids =
      tether_host_response_recorder_->GetPreviouslyConnectedHostIds();
  return base::Contains(connected_device_ids, device_id);
}

}  // namespace tether

}  // namespace ash

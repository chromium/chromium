// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_scan_device_prioritizer_impl.h"

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/tether/pref_names.h"
#include "chromeos/ash/components/tether/tether_host_response_recorder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace tether {

namespace {

// Returns true if |remote_device1| should be ordered before |remote_device2|.
bool CompareRemoteDevices(multidevice::RemoteDeviceRef remote_device1,
                          multidevice::RemoteDeviceRef remote_device2) {
  return remote_device1.last_update_time_millis() >
         remote_device2.last_update_time_millis();
}

void SortRemoteDevicesByLastUpdateTime(
    multidevice::RemoteDeviceRefList* remote_devices) {
  std::sort(remote_devices->begin(), remote_devices->end(),
            &CompareRemoteDevices);
}

}  // namespace

HostScanDevicePrioritizerImpl::HostScanDevicePrioritizerImpl(
    TetherHostResponseRecorder* tether_host_response_recorder)
    : tether_host_response_recorder_(tether_host_response_recorder) {}

HostScanDevicePrioritizerImpl::~HostScanDevicePrioritizerImpl() = default;

void HostScanDevicePrioritizerImpl::SortByHostScanOrder(
    multidevice::RemoteDeviceRefList* remote_devices) const {
  // First, fetch the hosts which have previously responded.
  std::vector<std::string> prioritized_ids =
      tether_host_response_recorder_->GetPreviouslyAvailableHostIds();

  std::vector<std::string> previously_connectable_host_ids =
      tether_host_response_recorder_->GetPreviouslyConnectedHostIds();
  if (!previously_connectable_host_ids.empty()) {
    // If there is a most-recently connectable host, insert it at the front of
    // the list.
    prioritized_ids.insert(prioritized_ids.begin(),
                           previously_connectable_host_ids[0]);
  }

  SortRemoteDevicesByLastUpdateTime(remote_devices);

  // Iterate from the last stored ID to the first stored ID. This ensures that
  // the items at the front of the list end up in the front of the prioritized
  // |remote_devices| vector.
  for (const std::string& prioritized_id : base::Reversed(prioritized_ids)) {
    // Iterate through |remote_devices| to see if a device ID exists which is
    // equal to |stored_id|. If one exists, remove it from its previous
    // position in the list and add it at the front instead.
    for (auto remote_devices_it = remote_devices->begin();
         remote_devices_it != remote_devices->end(); ++remote_devices_it) {
      if (remote_devices_it->GetDeviceId() != prioritized_id) {
        continue;
      }

      multidevice::RemoteDeviceRef device_to_move = *remote_devices_it;
      remote_devices->erase(remote_devices_it);
      remote_devices->emplace(remote_devices->begin(), device_to_move);
      break;
    }
  }
}

}  // namespace tether

}  // namespace ash

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/gms_core_notifications_state_tracker_impl.h"

#include <sstream>

#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::tether {

namespace {

bool ContainsDeviceWithId(
    const std::string& device_id,
    const std::vector<ScannedDeviceInfo>& device_info_list) {
  for (const auto& device_info : device_info_list) {
    if (device_info.device_id == device_id) {
      return true;
    }
  }

  return false;
}

}  // namespace

GmsCoreNotificationsStateTrackerImpl::GmsCoreNotificationsStateTrackerImpl() =
    default;

GmsCoreNotificationsStateTrackerImpl::~GmsCoreNotificationsStateTrackerImpl() {
  bool was_empty = device_id_to_name_map_.empty();
  device_id_to_name_map_.clear();

  if (!was_empty) {
    SendDeviceNamesChangeEvent();
  }
}

std::vector<std::string> GmsCoreNotificationsStateTrackerImpl::
    GetGmsCoreNotificationsDisabledDeviceNames() {
  std::vector<std::string> device_names;
  for (const auto& map_entry : device_id_to_name_map_) {
    device_names.push_back(map_entry.second);
  }
  return device_names;
}

void GmsCoreNotificationsStateTrackerImpl::OnTetherAvailabilityResponse(
    const std::vector<ScannedDeviceInfo>& scanned_device_list_so_far,
    const std::vector<ScannedDeviceInfo>&
        gms_core_notifications_disabled_devices,
    bool is_final_scan_result) {
  size_t old_size = device_id_to_name_map_.size();

  // Insert all names gathered by this scan to |device_id_to_name_map_|.
  for (const auto& remote_device : gms_core_notifications_disabled_devices) {
    device_id_to_name_map_[remote_device.device_id] = remote_device.device_name;
  }

  bool names_changed = old_size < device_id_to_name_map_.size();

  // Iterate through |device_id_to_name_map_| and remove entries which are no
  // longer valid given the newest scan results.
  auto it = device_id_to_name_map_.begin();
  while (it != device_id_to_name_map_.end()) {
    // A device has enabled notifications if it is included in the list of
    // scanned devices (only valid tether hosts are present in the list).
    bool device_enabled_notifications =
        ContainsDeviceWithId(it->first, scanned_device_list_so_far);

    // If this is the final scan result for this scan session and
    // |gms_core_notifications_disabled_devices| does not contain a given
    // device, the device was not found in the scan, and there is no way of
    // knowing whether that device has its notifications enabled or not.
    bool device_no_longer_found =
        is_final_scan_result &&
        !ContainsDeviceWithId(it->first,
                              gms_core_notifications_disabled_devices);

    if (device_enabled_notifications || device_no_longer_found) {
      it = device_id_to_name_map_.erase(it);
      names_changed = true;
    } else {
      ++it;
    }
  }

  if (names_changed) {
    SendDeviceNamesChangeEvent();
  }
}

void GmsCoreNotificationsStateTrackerImpl::SendDeviceNamesChangeEvent() {
  std::stringstream ss;
  ss << "GmsCore notifications disabled device list changed. Current list: [";

  if (!device_id_to_name_map_.empty()) {
    for (const auto& map_entry : device_id_to_name_map_) {
      ss << "{name: \"" << map_entry.second << "\", id: \""
         << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                map_entry.first)
         << "\"},";
    }
    // Move backward one character so that the final trailing comma will be
    // replaced by the ']' character below.
    ss.seekp(-1, ss.cur);
  }
  ss << "]";
  PA_LOG(VERBOSE) << ss.str();

  NotifyGmsCoreNotificationStateChanged();
}

}  // namespace ash::tether

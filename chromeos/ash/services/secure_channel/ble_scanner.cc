// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/ble_scanner.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash::secure_channel {

BleScanner::BleScanner() = default;

BleScanner::~BleScanner() = default;

void BleScanner::AddScanRequest(const ConnectionAttemptDetails& scan_request) {
  if (base::Contains(scan_requests_, scan_request)) {
    PA_LOG(ERROR) << "BleScanner::AddScanRequest(): Tried to add a scan "
                  << "request which already existed: " << scan_request;
    NOTREACHED();
  }

  scan_requests_.insert(scan_request);
  HandleScanRequestChange();
}

void BleScanner::RemoveScanRequest(
    const ConnectionAttemptDetails& scan_request) {
  if (!base::Contains(scan_requests_, scan_request)) {
    PA_LOG(ERROR) << "BleScanner::RemoveScanRequest(): Tried to remove a scan "
                  << "request which was not present: " << scan_request;
    NOTREACHED();
  }

  scan_requests_.erase(scan_request);
  HandleScanRequestChange();
}

bool BleScanner::HasScanRequest(const ConnectionAttemptDetails& scan_request) {
  return base::Contains(scan_requests_, scan_request);
}

void BleScanner::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BleScanner::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

DeviceIdPairSet BleScanner::GetAllDeviceIdPairs() {
  DeviceIdPairSet set;
  for (const auto& scan_request : scan_requests_)
    set.insert(scan_request.device_id_pair());
  return set;
}

void BleScanner::NotifyReceivedAdvertisementFromDevice(
    const multidevice::RemoteDeviceRef& remote_device,
    device::BluetoothDevice* bluetooth_device,
    ConnectionMedium connection_medium,
    ConnectionRole connection_role,
    const std::vector<uint8_t>& eid) {
  remote_device_id_to_last_seen_timestamp_[remote_device.GetDeviceId()] =
      base::Time::Now();

  for (auto& observer : observer_list_) {
    observer.OnReceivedAdvertisement(remote_device, bluetooth_device,
                                     connection_medium, connection_role, eid);
  }
}

absl::optional<base::Time> BleScanner::GetLastSeenTimestamp(
    const std::string& remote_device_id) {
  auto it = remote_device_id_to_last_seen_timestamp_.find(remote_device_id);
  if (it == remote_device_id_to_last_seen_timestamp_.end()) {
    return absl::nullopt;
  }

  return it->second;
}

}  // namespace ash::secure_channel

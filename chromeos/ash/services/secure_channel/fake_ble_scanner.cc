// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_ble_scanner.h"

namespace ash::secure_channel {

FakeBleScanner::FakeBleScanner() = default;

FakeBleScanner::~FakeBleScanner() = default;

std::vector<ConnectionAttemptDetails>
FakeBleScanner::GetAllScanRequestsForRemoteDevice(
    const std::string& remote_device_id) {
  std::vector<ConnectionAttemptDetails> all_scan_requests_for_remote_device;
  for (const auto& scan_request : scan_requests()) {
    if (scan_request.remote_device_id() == remote_device_id)
      all_scan_requests_for_remote_device.push_back(scan_request);
  }
  return all_scan_requests_for_remote_device;
}

void FakeBleScanner::HandleScanRequestChange() {
  ++num_scan_request_changes_handled_;
}

FakeBleScannerObserver::Result::Result(
    multidevice::RemoteDeviceRef remote_device,
    device::BluetoothDevice* bluetooth_device,
    ConnectionMedium connection_medium,
    ConnectionRole connection_role)
    : remote_device(remote_device),
      bluetooth_device(bluetooth_device),
      connection_medium(connection_medium),
      connection_role(connection_role) {}

FakeBleScannerObserver::Result::~Result() = default;

FakeBleScannerObserver::FakeBleScannerObserver() = default;

FakeBleScannerObserver::~FakeBleScannerObserver() = default;

void FakeBleScannerObserver::OnReceivedAdvertisement(
    multidevice::RemoteDeviceRef remote_device,
    device::BluetoothDevice* bluetooth_device,
    ConnectionMedium connection_medium,
    ConnectionRole connection_role,
    const std::vector<uint8_t>& eid) {
  handled_scan_results_.emplace_back(remote_device, bluetooth_device,
                                     connection_medium, connection_role);
}

}  // namespace ash::secure_channel

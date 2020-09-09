// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_ble_scanner.h"

namespace chromeos {

namespace secure_channel {

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

FakeBleScannerObserver::FakeBleScannerObserver() = default;

FakeBleScannerObserver::~FakeBleScannerObserver() = default;

void FakeBleScannerObserver::OnReceivedAdvertisement(
    multidevice::RemoteDeviceRef remote_device,
    device::BluetoothDevice* bluetooth_device,
    ConnectionRole connection_role) {
  handled_scan_results_.push_back(
      std::make_tuple(remote_device, bluetooth_device, connection_role));
}

}  // namespace secure_channel

}  // namespace chromeos

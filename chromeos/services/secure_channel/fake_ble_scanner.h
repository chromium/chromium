// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_SCANNER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_SCANNER_H_

#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_scanner.h"
#include "chromeos/services/secure_channel/device_id_pair.h"

namespace chromeos {

namespace secure_channel {

// Test BleScanner implementation.
class FakeBleScanner : public BleScanner {
 public:
  FakeBleScanner();
  ~FakeBleScanner() override;

  size_t num_scan_request_changes_handled() const {
    return num_scan_request_changes_handled_;
  }

  std::vector<ConnectionAttemptDetails> GetAllScanRequestsForRemoteDevice(
      const std::string& remote_device_id);

  // Public for testing.
  using BleScanner::NotifyReceivedAdvertisementFromDevice;
  using BleScanner::scan_requests;

 private:
  void HandleScanRequestChange() override;

  size_t num_scan_request_changes_handled_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(FakeBleScanner);
};

// Test BleScanner::Observer implementation.
class FakeBleScannerObserver : public BleScanner::Observer {
 public:
  struct Result {
    Result(multidevice::RemoteDeviceRef remote_device,
           device::BluetoothDevice* bluetooth_device,
           ConnectionMedium connection_medium,
           ConnectionRole connection_role);
    ~Result();

    multidevice::RemoteDeviceRef remote_device;
    device::BluetoothDevice* bluetooth_device;
    ConnectionMedium connection_medium;
    ConnectionRole connection_role;
  };

  FakeBleScannerObserver();
  ~FakeBleScannerObserver() override;

  const std::vector<Result>& handled_scan_results() const {
    return handled_scan_results_;
  }

 private:
  void OnReceivedAdvertisement(multidevice::RemoteDeviceRef remote_device,
                               device::BluetoothDevice* bluetooth_device,
                               ConnectionMedium connection_medium,
                               ConnectionRole connection_role) override;

  std::vector<Result> handled_scan_results_;

  DISALLOW_COPY_AND_ASSIGN(FakeBleScannerObserver);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_SCANNER_H_

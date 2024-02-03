// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_SCANNER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_SCANNER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/ble_scanner.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"

namespace ash::secure_channel {

// Test BleScanner implementation.
class FakeBleScanner : public BleScanner {
 public:
  FakeBleScanner();

  FakeBleScanner(const FakeBleScanner&) = delete;
  FakeBleScanner& operator=(const FakeBleScanner&) = delete;

  ~FakeBleScanner() override;

  size_t num_scan_request_changes_handled() const {
    return num_scan_request_changes_handled_;
  }

  std::vector<ConnectionAttemptDetails> GetAllScanRequestsForRemoteDevice(
      const std::string& remote_device_id);

  // Public for testing.
  using BleScanner::NotifyBleDiscoverySessionFailed;
  using BleScanner::NotifyReceivedAdvertisementFromDevice;
  using BleScanner::scan_requests;

 private:
  void HandleScanRequestChange() override;

  size_t num_scan_request_changes_handled_ = 0u;
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
    raw_ptr<device::BluetoothDevice, DanglingUntriaged> bluetooth_device;
    ConnectionMedium connection_medium;
    ConnectionRole connection_role;
  };

  FakeBleScannerObserver();

  FakeBleScannerObserver(const FakeBleScannerObserver&) = delete;
  FakeBleScannerObserver& operator=(const FakeBleScannerObserver&) = delete;

  ~FakeBleScannerObserver() override;

  const std::vector<Result>& handled_scan_results() const {
    return handled_scan_results_;
  }

 private:
  void OnReceivedAdvertisement(multidevice::RemoteDeviceRef remote_device,
                               device::BluetoothDevice* bluetooth_device,
                               ConnectionMedium connection_medium,
                               ConnectionRole connection_role,
                               const std::vector<uint8_t>& eid) override;

  std::vector<Result> handled_scan_results_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_SCANNER_H_

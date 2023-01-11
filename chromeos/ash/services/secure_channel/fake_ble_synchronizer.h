// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_SYNCHRONIZER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_SYNCHRONIZER_H_

#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/secure_channel/ble_synchronizer_base.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace ash::secure_channel {

// Test double for BleSynchronizer.
class FakeBleSynchronizer : public BleSynchronizerBase {
 public:
  FakeBleSynchronizer();

  FakeBleSynchronizer(const FakeBleSynchronizer&) = delete;
  FakeBleSynchronizer& operator=(const FakeBleSynchronizer&) = delete;

  ~FakeBleSynchronizer() override;

  size_t GetNumCommands();

  device::BluetoothAdvertisement::Data& GetAdvertisementData(size_t index);
  device::BluetoothAdapter::CreateAdvertisementCallback GetRegisterCallback(
      size_t index);
  device::BluetoothAdapter::AdvertisementErrorCallback GetRegisterErrorCallback(
      size_t index);

  device::BluetoothAdvertisement::SuccessCallback GetUnregisterCallback(
      size_t index);
  device::BluetoothAdvertisement::ErrorCallback GetUnregisterErrorCallback(
      size_t index);

  device::BluetoothAdapter::DiscoverySessionCallback TakeStartDiscoveryCallback(
      size_t index);
  device::BluetoothAdapter::ErrorCallback TakeStartDiscoveryErrorCallback(
      size_t index);

  base::OnceClosure GetStopDiscoveryCallback(size_t index);
  device::BluetoothDiscoverySession::ErrorCallback
  GetStopDiscoveryErrorCallback(size_t index);

 protected:
  void ProcessQueue() override;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_SYNCHRONIZER_H_

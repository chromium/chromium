// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/bluetooth_device_status_notifier.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "components/device_event_log/device_event_log.h"

namespace ash::bluetooth_config {

BluetoothDeviceStatusNotifier::BluetoothDeviceStatusNotifier() = default;

BluetoothDeviceStatusNotifier::~BluetoothDeviceStatusNotifier() = default;

void BluetoothDeviceStatusNotifier::ObserveDeviceStatusChanges(
    mojo::PendingRemote<mojom::BluetoothDeviceStatusObserver> observer) {
  observers_.Add(std::move(observer));
}

void BluetoothDeviceStatusNotifier::NotifyDeviceNewlyPaired(
    const mojom::PairedBluetoothDevicePropertiesPtr& device) {
  BLUETOOTH_LOG(EVENT) << "Notifying observers device "
                       << GetPairedDeviceName(device) << " is newly paired";
  for (auto& observer : observers_) {
    observer->OnDevicePaired(mojo::Clone(device));
  }
}

void BluetoothDeviceStatusNotifier::NotifyDeviceNewlyConnected(
    const mojom::PairedBluetoothDevicePropertiesPtr& device) {
  BLUETOOTH_LOG(EVENT) << "Notifying observers device "
                       << GetPairedDeviceName(device) << " is newly connected";
  for (auto& observer : observers_) {
    observer->OnDeviceConnected(mojo::Clone(device));
  }
}

void BluetoothDeviceStatusNotifier::NotifyDeviceNewlyDisconnected(
    const mojom::PairedBluetoothDevicePropertiesPtr& device) {
  BLUETOOTH_LOG(EVENT) << "Notifying observers device "
                       << GetPairedDeviceName(device)
                       << " is newly disconnected";
  for (auto& observer : observers_) {
    observer->OnDeviceDisconnected(mojo::Clone(device));
  }
}

void BluetoothDeviceStatusNotifier::FlushForTesting() {
  observers_.FlushForTesting();  // IN-TEST
}

}  // namespace ash::bluetooth_config

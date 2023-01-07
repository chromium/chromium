// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_DEVICE_STATUS_NOTIFIER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_DEVICE_STATUS_NOTIFIER_H_

#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::bluetooth_config {

// Notifiers all listeners of changes in devices status.
// Status changes includes a newly paired device, connection and
// disconnection.
class BluetoothDeviceStatusNotifier {
 public:
  virtual ~BluetoothDeviceStatusNotifier();

  // Adds an observer of Bluetooth device status. |observer| will be notified
  // each time Bluetooth device status changes. To stop observing, clients
  // should disconnect the Mojo pipe to |observer| by deleting the associated
  // Receiver.
  void ObserveDeviceStatusChanges(
      mojo::PendingRemote<mojom::BluetoothDeviceStatusObserver> observer);

 protected:
  BluetoothDeviceStatusNotifier();

  // Notifies all observers for a device that is newly paired. Should be
  // called by derived types to notify observers of device pairings.
  void NotifyDeviceNewlyPaired(
      const mojom::PairedBluetoothDevicePropertiesPtr& device);

  // Notifies all observers for a device that is connected. Should be
  // called by derived types to notify observers of device connection.
  void NotifyDeviceNewlyConnected(
      const mojom::PairedBluetoothDevicePropertiesPtr& device);

  // Notifies all observers for a device that is disconnected. Should be
  // called by derived types to notify observers of device disconnection.
  void NotifyDeviceNewlyDisconnected(
      const mojom::PairedBluetoothDevicePropertiesPtr& device);

 private:
  friend class BluetoothDeviceStatusNotifierImplTest;

  // Flushes queued Mojo messages in unit tests.
  void FlushForTesting();

  mojo::RemoteSet<mojom::BluetoothDeviceStatusObserver> observers_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_BLUETOOTH_DEVICE_STATUS_NOTIFIER_H_

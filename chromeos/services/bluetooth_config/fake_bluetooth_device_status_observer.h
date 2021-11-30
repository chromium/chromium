// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DEVICE_STATUS_OBSERVER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DEVICE_STATUS_OBSERVER_H_

#include <vector>

#include "base/run_loop.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace bluetooth_config {

class FakeBluetoothDeviceStatusObserver
    : public mojom::BluetoothDeviceStatusObserver {
 public:
  FakeBluetoothDeviceStatusObserver();
  ~FakeBluetoothDeviceStatusObserver() override;

  // Generates a PendingRemote associated with this object. To disconnect the
  // associated Mojo pipe, use DisconnectMojoPipe().
  mojo::PendingRemote<mojom::BluetoothDeviceStatusObserver>
  GeneratePendingRemote();

  // Disconnects the Mojo pipe associated with a PendingRemote returned by
  // GeneratePendingRemote().
  void DisconnectMojoPipe();

  const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
  paired_device_properties_list() const {
    return paired_device_properties_list_;
  }

  const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
  connected_device_properties_list() const {
    return connected_device_properties_list_;
  }

  const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
  disconnected_device_properties_list() const {
    return disconnected_device_properties_list_;
  }

 private:
  // mojom::BluetoothDeviceStatusObserver:
  void OnDevicePaired(
      mojom::PairedBluetoothDevicePropertiesPtr device) override;
  void OnDeviceConnected(
      mojom::PairedBluetoothDevicePropertiesPtr device) override;
  void OnDeviceDisconnected(
      mojom::PairedBluetoothDevicePropertiesPtr device) override;

  std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
      paired_device_properties_list_;
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
      connected_device_properties_list_;
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
      disconnected_device_properties_list_;
  mojo::Receiver<mojom::BluetoothDeviceStatusObserver> receiver_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DEVICE_STATUS_OBSERVER_H_

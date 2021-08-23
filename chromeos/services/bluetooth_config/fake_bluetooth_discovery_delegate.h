// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DISCOVERY_DELEGATE_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DISCOVERY_DELEGATE_H_

#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace bluetooth_config {

class FakeBluetoothDiscoveryDelegate
    : public mojom::BluetoothDiscoveryDelegate {
 public:
  FakeBluetoothDiscoveryDelegate();
  ~FakeBluetoothDiscoveryDelegate() override;

  // Generates a PendingRemote associated with this object. To disconnect the
  // associated Mojo pipe, use DisconnectMojoPipe().
  mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate>
  GeneratePendingRemote();

  // Disconnects the Mojo pipe associated with a PendingRemote returned by
  // GeneratePendingRemote().
  void DisconnectMojoPipe();

  bool IsMojoPipeConnected() const;

  size_t num_start_callbacks() const { return num_start_callbacks_; }
  size_t num_stop_callbacks() const { return num_stop_callbacks_; }

 private:
  // mojom::BluetoothDiscoveryDelegate:
  void OnBluetoothDiscoveryStarted() override;
  void OnBluetoothDiscoveryStopped() override;

  void OnDisconnected();

  size_t num_start_callbacks_ = 0u;
  size_t num_stop_callbacks_ = 0u;

  mojo::Receiver<mojom::BluetoothDiscoveryDelegate> receiver_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_BLUETOOTH_DISCOVERY_DELEGATE_H_

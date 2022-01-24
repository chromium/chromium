// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_SYSTEM_PROPERTIES_OBSERVER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_SYSTEM_PROPERTIES_OBSERVER_H_

#include <vector>

#include "base/run_loop.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace bluetooth_config {

class FakeSystemPropertiesObserver : public mojom::SystemPropertiesObserver {
 public:
  FakeSystemPropertiesObserver();
  ~FakeSystemPropertiesObserver() override;

  // Generates a PendingRemote associated with this object. To disconnect the
  // associated Mojo pipe, use DisconnectMojoPipe().
  mojo::PendingRemote<mojom::SystemPropertiesObserver> GeneratePendingRemote();

  // Disconnects the Mojo pipe associated with a PendingRemote returned by
  // GeneratePendingRemote().
  void DisconnectMojoPipe();

  const std::vector<mojom::BluetoothSystemPropertiesPtr>&
  received_properties_list() const {
    return received_properties_list_;
  }

 private:
  // mojom::SystemPropertiesObserver:
  void OnPropertiesUpdated(
      mojom::BluetoothSystemPropertiesPtr properties) override;

  std::vector<mojom::BluetoothSystemPropertiesPtr> received_properties_list_;
  mojo::Receiver<mojom::SystemPropertiesObserver> receiver_{this};
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_SYSTEM_PROPERTIES_OBSERVER_H_

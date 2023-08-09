// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_H_

#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::bluetooth_config {

// Provides system Bluetooth properties, including Bluetooth availability and
// on/off state.
class SystemPropertiesProvider {
 public:
  virtual ~SystemPropertiesProvider();

  // Adds an observer of system properties. |observer| will be notified of
  // the current properties immediately as a result of this function, then again
  // each time system properties change. To stop observing, clients should
  // disconnect the Mojo pipe to |observer| by deleting the associated Receiver.
  void Observe(mojo::PendingRemote<mojom::SystemPropertiesObserver> observer);

 protected:
  SystemPropertiesProvider();

  virtual mojom::BluetoothSystemState ComputeSystemState() const = 0;
  virtual mojom::BluetoothModificationState ComputeModificationState()
      const = 0;
  virtual std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
  GetPairedDevices() const = 0;
  virtual std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
  GetFastPairableDevices() const = 0;

  // Notifies all observers of property changes; should be called by derived
  // types to notify observers of property changes.
  void NotifyPropertiesChanged();

 private:
  friend class SystemPropertiesProviderImplTest;

  mojom::BluetoothSystemPropertiesPtr GenerateProperties();

  // Flushes queued Mojo messages in unit tests.
  void FlushForTesting();

  void NotifyObserver(mojom::SystemPropertiesObserver* observer,
                      mojom::BluetoothSystemPropertiesPtr properties);

  mojo::RemoteSet<mojom::SystemPropertiesObserver> observers_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_SYSTEM_PROPERTIES_PROVIDER_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_CROS_BLUETOOTH_CONFIG_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_CROS_BLUETOOTH_CONFIG_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace chromeos {
namespace bluetooth_config {

class AdapterStateController;
class DeviceCache;
class DeviceOperationHandler;
class DiscoverySessionManager;
class Initializer;
class SystemPropertiesProvider;

// Implements the CrosNetworkConfig API, which is used to support Bluetooth
// system UI on Chrome OS. This class instantiates helper classes and implements
// the API by delegating to these helpers.
class CrosBluetoothConfig : public mojom::CrosBluetoothConfig {
 public:
  CrosBluetoothConfig(
      Initializer& initializer,
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  ~CrosBluetoothConfig() override;

  // Binds a PendingReceiver to this instance. Clients wishing to use the
  // CrosBluetoothConfig API should use this function as an entrypoint.
  void BindPendingReceiver(
      mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver);

 private:
  // mojom::CrosBluetoothConfig:
  void ObserveSystemProperties(
      mojo::PendingRemote<mojom::SystemPropertiesObserver> observer) override;
  void SetBluetoothEnabledState(bool enabled) override;
  void StartDiscovery(
      mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate> delegate) override;
  void Connect(const std::string& device_id, ConnectCallback callback) override;
  void Disconnect(const std::string& device_id,
                  DisconnectCallback callback) override;
  void Forget(const std::string& device_id, ForgetCallback callback) override;

  mojo::ReceiverSet<mojom::CrosBluetoothConfig> receivers_;

  std::unique_ptr<AdapterStateController> adapter_state_controller_;
  std::unique_ptr<DeviceCache> device_cache_;
  std::unique_ptr<SystemPropertiesProvider> system_properties_provider_;
  std::unique_ptr<DiscoverySessionManager> discovery_session_manager_;
  std::unique_ptr<DeviceOperationHandler> device_operation_handler_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_CROS_BLUETOOTH_CONFIG_H_

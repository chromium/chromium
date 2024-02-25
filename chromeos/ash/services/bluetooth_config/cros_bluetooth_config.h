// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_CROS_BLUETOOTH_CONFIG_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_CROS_BLUETOOTH_CONFIG_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

class PrefService;

namespace ash::bluetooth_config {

class AdapterStateController;
class BluetoothDeviceStatusNotifier;
class BluetoothPowerController;
class DeviceCache;
class DeviceNameManager;
class DeviceOperationHandler;
class DiscoveredDevicesProvider;
class DiscoverySessionManager;
class FastPairDelegate;
class Initializer;
class SystemPropertiesProvider;

// Implements the CrosNetworkConfig API, which is used to support Bluetooth
// system UI on Chrome OS. This class instantiates helper classes and implements
// the API by delegating to these helpers.
class CrosBluetoothConfig : public mojom::CrosBluetoothConfig {
 public:
  CrosBluetoothConfig(Initializer& initializer,
                      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
                      FastPairDelegate* fast_pair_delegate);
  ~CrosBluetoothConfig() override;

  // Sets the PrefServices to be used by classes within CrosBluetoothConfig.
  void SetPrefs(PrefService* logged_in_profile_prefs, PrefService* local_state);

  // Binds a PendingReceiver to this instance. Clients wishing to use the
  // CrosBluetoothConfig API should use this function as an entrypoint.
  void BindPendingReceiver(
      mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver);

 private:
  // mojom::CrosBluetoothConfig:
  void ObserveSystemProperties(
      mojo::PendingRemote<mojom::SystemPropertiesObserver> observer) override;
  void ObserveDeviceStatusChanges(
      mojo::PendingRemote<mojom::BluetoothDeviceStatusObserver> observer)
      override;
  void ObserveDiscoverySessionStatusChanges(
      mojo::PendingRemote<mojom::DiscoverySessionStatusObserver> observer)
      override;
  void SetBluetoothEnabledState(bool enabled) override;
  void SetBluetoothEnabledWithoutPersistence() override;
  void SetBluetoothHidDetectionInactive(bool is_using_bluetooth) override;
  void StartDiscovery(
      mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate> delegate) override;
  void Connect(const std::string& device_id, ConnectCallback callback) override;
  void Disconnect(const std::string& device_id,
                  DisconnectCallback callback) override;
  void Forget(const std::string& device_id, ForgetCallback callback) override;
  void SetDeviceNickname(const std::string& device_id,
                         const std::string& nickname) override;

  mojo::ReceiverSet<mojom::CrosBluetoothConfig> receivers_;

  std::unique_ptr<AdapterStateController> adapter_state_controller_;
  std::unique_ptr<BluetoothPowerController> bluetooth_power_controller_;
  std::unique_ptr<DeviceNameManager> device_name_manager_;
  std::unique_ptr<DeviceCache> device_cache_;
  std::unique_ptr<SystemPropertiesProvider> system_properties_provider_;
  std::unique_ptr<BluetoothDeviceStatusNotifier>
      bluetooth_device_status_notifier_;
  std::unique_ptr<DiscoveredDevicesProvider> discovered_devices_provider_;
  std::unique_ptr<DiscoverySessionManager> discovery_session_manager_;
  std::unique_ptr<DeviceOperationHandler> device_operation_handler_;
  raw_ptr<FastPairDelegate> fast_pair_delegate_ = nullptr;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_CROS_BLUETOOTH_CONFIG_H_

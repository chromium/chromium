// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::phonehub {

// FeatureStatusProvider implementation which utilizes DeviceSyncClient,
// MultiDeviceSetupClient and BluetoothAdapter to determine the current status.
class FeatureStatusProviderImpl
    : public FeatureStatusProvider,
      public device_sync::DeviceSyncClient::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public device::BluetoothAdapter::Observer,
      public secure_channel::ConnectionManager::Observer,
      public session_manager::SessionManagerObserver,
      public chromeos::PowerManagerClient::Observer {
 public:
  FeatureStatusProviderImpl(
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      secure_channel::ConnectionManager* connection_manager,
      session_manager::SessionManager* session_manager,
      chromeos::PowerManagerClient* power_manager_client,
      PhoneHubStructuredMetricsLogger* phone_hub_structured_metrics_logger);
  ~FeatureStatusProviderImpl() override;

 private:
  friend class FeatureStatusProviderImplTest;

  // FeatureStatusProvider:
  FeatureStatus GetStatus() const override;

  // device_sync::DeviceSyncClient::Observer:
  void OnReady() override;
  void OnNewDevicesSynced() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  void OnBluetoothAdapterReceived(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  void UpdateStatus();
  FeatureStatus ComputeStatus();
  bool IsBluetoothOn() const;

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  // SessionManagerObserver:
  void OnSessionStateChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  void CheckEligibleDevicesForNudge();

  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_;
  raw_ptr<multidevice_setup::MultiDeviceSetupClient> multidevice_setup_client_;
  raw_ptr<secure_channel::ConnectionManager> connection_manager_;
  raw_ptr<session_manager::SessionManager> session_manager_;
  raw_ptr<chromeos::PowerManagerClient> power_manager_client_;
  raw_ptr<PhoneHubStructuredMetricsLogger> phone_hub_structured_metrics_logger_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  std::optional<FeatureStatus> status_;
  bool is_suspended_ = false;

  base::WeakPtrFactory<FeatureStatusProviderImpl> weak_ptr_factory_{this};
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_IMPL_H_

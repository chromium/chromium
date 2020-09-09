// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/phonehub/connection_manager.h"
#include "chromeos/components/phonehub/feature_status_provider.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {

namespace phonehub {

// FeatureStatusProvider implementation which utilizes DeviceSyncClient,
// MultiDeviceSetupClient and BluetoothAdapter to determine the current status.
// TODO(khorimoto): Add metrics for initial status and status changes.
class FeatureStatusProviderImpl
    : public FeatureStatusProvider,
      public device_sync::DeviceSyncClient::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer,
      public device::BluetoothAdapter::Observer,
      public ConnectionManager::Observer {
 public:
  FeatureStatusProviderImpl(
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      ConnectionManager* connection_manager);
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

  // ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  device_sync::DeviceSyncClient* device_sync_client_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  ConnectionManager* connection_manager_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  base::Optional<FeatureStatus> status_;

  base::WeakPtrFactory<FeatureStatusProviderImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FEATURE_STATUS_PROVIDER_IMPL_H_

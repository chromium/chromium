// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_FEATURE_STATUS_PROVIDER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_FEATURE_STATUS_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/components/eche_app_ui/feature_status_provider.h"
#include "chromeos/components/phonehub/feature_status_provider.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_manager.h"

namespace chromeos {
namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync
namespace phonehub {
class PhoneHubManager;
}  // namespace phonehub
namespace eche_app {

// FeatureStatusProvider implementation which observes PhoneHub's state, then
// layers in Eche's state.
class EcheFeatureStatusProvider
    : public FeatureStatusProvider,
      public phonehub::FeatureStatusProvider::Observer,
      public secure_channel::ConnectionManager::Observer,
      public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  EcheFeatureStatusProvider(
      phonehub::PhoneHubManager* phone_hub_manager,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      secure_channel::ConnectionManager* connection_manager);
  ~EcheFeatureStatusProvider() override;

  // FeatureStatusProvider:
  FeatureStatus GetStatus() const override;

 private:
  void UpdateStatus();
  FeatureStatus ComputeStatus();

  // phonehub::FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // secure_channel::ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnHostStatusChanged(
      const multidevice_setup::MultiDeviceSetupClient::HostStatusWithDevice&
          host_device_with_status) override;
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  phonehub::FeatureStatusProvider* phone_hub_feature_status_provider_;
  device_sync::DeviceSyncClient* device_sync_client_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  secure_channel::ConnectionManager* connection_manager_;
  phonehub::FeatureStatus current_phone_hub_feature_status_;
  base::Optional<FeatureStatus> status_;
  base::WeakPtrFactory<EcheFeatureStatusProvider> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_FEATURE_STATUS_PROVIDER_H_

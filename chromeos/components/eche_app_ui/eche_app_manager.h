// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_APP_MANAGER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_APP_MANAGER_H_

#include <stdint.h>

#include "chromeos/components/eche_app_ui/eche_connector.h"
#include "chromeos/components/eche_app_ui/eche_feature_status_provider.h"
#include "chromeos/components/eche_app_ui/eche_notification_click_handler.h"
#include "chromeos/components/eche_app_ui/eche_recent_app_click_handler.h"
#include "chromeos/components/eche_app_ui/launch_app_helper.h"
#include "chromeos/components/eche_app_ui/mojom/eche_app.mojom.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace multidevice_setup {
class MultiDeviceSetupClient;
}  // namespace multidevice_setup

namespace secure_channel {
class ConnectionManager;
class SecureChannelClient;
}  // namespace secure_channel

namespace eche_app {

class SystemInfo;
class EcheSignaler;
class SystemInfoProvider;
class EcheUidProvider;
class EcheNotificationGenerator;
class EcheMessageReceiver;

// Implements the core logic of the EcheApp and exposes interfaces via its
// public API. Implemented as a KeyedService since it depends on other
// KeyedService instances.
class EcheAppManager : public KeyedService {
 public:
  EcheAppManager(PrefService* pref_service,
                 std::unique_ptr<SystemInfo> system_info,
                 phonehub::PhoneHubManager*,
                 device_sync::DeviceSyncClient*,
                 multidevice_setup::MultiDeviceSetupClient*,
                 secure_channel::SecureChannelClient*,
                 LaunchAppHelper::LaunchEcheAppFunction,
                 LaunchAppHelper::CloseEcheAppFunction,
                 LaunchAppHelper::LaunchNotificationFunction);
  ~EcheAppManager() override;

  EcheAppManager(const EcheAppManager&) = delete;
  EcheAppManager& operator=(const EcheAppManager&) = delete;

  void BindSignalingMessageExchangerInterface(
      mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver);

  void BindUidGeneratorInterface(
      mojo::PendingReceiver<mojom::UidGenerator> receiver);

  void BindSystemInfoProviderInterface(
      mojo::PendingReceiver<mojom::SystemInfoProvider> receiver);

  void BindNotificationGeneratorInterface(
      mojo::PendingReceiver<mojom::NotificationGenerator> receiver);

  // KeyedService:
  void Shutdown() override;

 private:
  std::unique_ptr<secure_channel::ConnectionManager> connection_manager_;
  std::unique_ptr<EcheFeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<LaunchAppHelper> launch_app_helper_;
  std::unique_ptr<EcheNotificationClickHandler>
      eche_notification_click_handler_;
  std::unique_ptr<EcheConnector> eche_connector_;
  std::unique_ptr<EcheSignaler> signaler_;
  std::unique_ptr<EcheUidProvider> uid_;
  std::unique_ptr<EcheRecentAppClickHandler> eche_recent_app_click_handler_;
  std::unique_ptr<EcheNotificationGenerator> notification_generator_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  std::unique_ptr<SystemInfoProvider> system_info_provider_;
  std::unique_ptr<EcheMessageReceiver> message_receiver_;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_APP_MANAGER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_

#include <memory>

#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace multidevice_setup {
class MultiDeviceSetupClient;
}  // namespace multidevice_setup

namespace phonehub {

// Implemented as a KeyedService which is keyed by the primary Profile.
class PhoneHubManagerImpl : public PhoneHubManager, public KeyedService {
 public:
  PhoneHubManagerImpl(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client);
  ~PhoneHubManagerImpl() override;

  // PhoneHubManager:
  DoNotDisturbController* GetDoNotDisturbController() override;
  FeatureStatusProvider* GetFeatureStatusProvider() override;
  NotificationAccessManager* GetNotificationAccessManager() override;
  NotificationManager* GetNotificationManager() override;
  PhoneModel* GetPhoneModel() override;
  TetherController* GetTetherController() override;

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<DoNotDisturbController> do_not_disturb_controller_;
  std::unique_ptr<FeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<NotificationAccessManager> notification_access_manager_;
  std::unique_ptr<NotificationManager> notification_manager_;
  std::unique_ptr<PhoneModel> phone_model_;
  std::unique_ptr<TetherController> tether_controller_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_

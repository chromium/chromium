// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_

#include <memory>

#include "base/callback.h"
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

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

namespace phonehub {

class ConnectionManager;
class MessageSender;
class MessageReceiver;
class MutablePhoneModel;
class PhoneStatusProcessor;

// Implemented as a KeyedService which is keyed by the primary Profile.
class PhoneHubManagerImpl : public PhoneHubManager, public KeyedService {
 public:
  PhoneHubManagerImpl(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      chromeos::secure_channel::SecureChannelClient* secure_channel_client,
      const base::RepeatingClosure& show_multidevice_setup_dialog_callback);
  ~PhoneHubManagerImpl() override;

  // PhoneHubManager:
  ConnectionScheduler* GetConnectionScheduler() override;
  DoNotDisturbController* GetDoNotDisturbController() override;
  FeatureStatusProvider* GetFeatureStatusProvider() override;
  FindMyDeviceController* GetFindMyDeviceController() override;
  NotificationAccessManager* GetNotificationAccessManager() override;
  NotificationManager* GetNotificationManager() override;
  OnboardingUiTracker* GetOnboardingUiTracker() override;
  PhoneModel* GetPhoneModel() override;
  TetherController* GetTetherController() override;

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<DoNotDisturbController> do_not_disturb_controller_;
  std::unique_ptr<ConnectionManager> connection_manager_;
  std::unique_ptr<FeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<MessageReceiver> message_receiver_;
  std::unique_ptr<MessageSender> message_sender_;
  std::unique_ptr<ConnectionScheduler> connection_scheduler_;
  std::unique_ptr<FindMyDeviceController> find_my_device_controller_;
  std::unique_ptr<NotificationAccessManager> notification_access_manager_;
  std::unique_ptr<NotificationManager> notification_manager_;
  std::unique_ptr<OnboardingUiTracker> onboarding_ui_tracker_;
  std::unique_ptr<MutablePhoneModel> phone_model_;
  std::unique_ptr<PhoneStatusProcessor> phone_status_processor_;
  std::unique_ptr<TetherController> tether_controller_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_

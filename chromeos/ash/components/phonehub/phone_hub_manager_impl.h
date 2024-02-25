// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/app_stream_manager.h"
#include "chromeos/ash/components/phonehub/feature_setup_response_processor.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/phone_hub_ui_readiness_recorder.h"
#include "chromeos/ash/components/phonehub/public/cpp/attestation_certificate_generator.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace ash {

namespace device_sync {
class DeviceSyncClient;
}

namespace multidevice_setup {
class MultiDeviceSetupClient;
}

namespace secure_channel {
class ConnectionManager;
class SecureChannelClient;
}  // namespace secure_channel

namespace phonehub {

class BrowserTabsModelController;
class BrowserTabsModelProvider;
class CameraRollDownloadManager;
class CameraRollManager;
class CrosStateSender;
class InvalidConnectionDisconnector;
class MessageReceiver;
class MessageSender;
class MultideviceSetupStateUpdater;
class MutablePhoneModel;
class NotificationProcessor;
class PhoneStatusProcessor;
class PingManager;
class UserActionRecorder;

// Implemented as a KeyedService which is keyed by the primary Profile.
class PhoneHubManagerImpl : public PhoneHubManager, public KeyedService {
 public:
  PhoneHubManagerImpl(
      PrefService* pref_service,
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      std::unique_ptr<BrowserTabsModelProvider> browser_tabs_model_provider,
      std::unique_ptr<CameraRollDownloadManager> camera_roll_download_manager,
      const base::RepeatingClosure& show_multidevice_setup_dialog_callback,
      std::unique_ptr<AttestationCertificateGenerator>
          attestation_certificate_generator);

  ~PhoneHubManagerImpl() override;

  // PhoneHubManager:
  BrowserTabsModelProvider* GetBrowserTabsModelProvider() override;
  CameraRollManager* GetCameraRollManager() override;
  ConnectionScheduler* GetConnectionScheduler() override;
  DoNotDisturbController* GetDoNotDisturbController() override;
  FeatureStatusProvider* GetFeatureStatusProvider() override;
  FindMyDeviceController* GetFindMyDeviceController() override;
  MultideviceFeatureAccessManager* GetMultideviceFeatureAccessManager()
      override;
  NotificationInteractionHandler* GetNotificationInteractionHandler() override;
  NotificationManager* GetNotificationManager() override;
  OnboardingUiTracker* GetOnboardingUiTracker() override;
  AppStreamLauncherDataModel* GetAppStreamLauncherDataModel() override;
  PhoneModel* GetPhoneModel() override;
  PingManager* GetPingManager() override;
  RecentAppsInteractionHandler* GetRecentAppsInteractionHandler() override;
  ScreenLockManager* GetScreenLockManager() override;
  TetherController* GetTetherController() override;
  UserActionRecorder* GetUserActionRecorder() override;
  IconDecoder* GetIconDecoder() override;
  AppStreamManager* GetAppStreamManager() override;
  PhoneHubUiReadinessRecorder* GetPhoneHubUiReadinessRecorder() override;

  void GetHostLastSeenTimestamp(
      base::OnceCallback<void(std::optional<base::Time>)> callback) override;

  eche_app::EcheConnectionStatusHandler* GetEcheConnectionStatusHandler()
      override;
  void SetEcheConnectionStatusHandler(
      eche_app::EcheConnectionStatusHandler* eche_connection_status_handler)
      override;
  void SetSystemInfoProvider(
      eche_app::SystemInfoProvider* system_info_provider) override;
  eche_app::SystemInfoProvider* GetSystemInfoProvider() override;
  PhoneHubStructuredMetricsLogger* GetPhoneHubStructuredMetricsLogger()
      override;

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<IconDecoder> icon_decoder_;
  std::unique_ptr<PhoneHubStructuredMetricsLogger>
      phone_hub_structured_metrics_logger_;
  std::unique_ptr<secure_channel::ConnectionManager> connection_manager_;
  std::unique_ptr<FeatureStatusProvider> feature_status_provider_;
  std::unique_ptr<UserActionRecorder> user_action_recorder_;
  std::unique_ptr<PhoneHubUiReadinessRecorder> phone_hub_ui_readiness_recorder_;
  std::unique_ptr<MessageReceiver> message_receiver_;
  std::unique_ptr<MessageSender> message_sender_;
  std::unique_ptr<MutablePhoneModel> phone_model_;
  std::unique_ptr<CrosStateSender> cros_state_sender_;
  std::unique_ptr<DoNotDisturbController> do_not_disturb_controller_;
  std::unique_ptr<ConnectionScheduler> connection_scheduler_;
  std::unique_ptr<FindMyDeviceController> find_my_device_controller_;
  std::unique_ptr<MultideviceFeatureAccessManager>
      multidevice_feature_access_manager_;
  std::unique_ptr<ScreenLockManager> screen_lock_manager_;
  std::unique_ptr<NotificationInteractionHandler>
      notification_interaction_handler_;
  std::unique_ptr<NotificationManager> notification_manager_;
  std::unique_ptr<OnboardingUiTracker> onboarding_ui_tracker_;
  std::unique_ptr<AppStreamLauncherDataModel> app_stream_launcher_data_model_;
  std::unique_ptr<NotificationProcessor> notification_processor_;
  std::unique_ptr<RecentAppsInteractionHandler>
      recent_apps_interaction_handler_;
  std::unique_ptr<AppStreamManager> app_stream_manager_;
  std::unique_ptr<PhoneStatusProcessor> phone_status_processor_;
  std::unique_ptr<TetherController> tether_controller_;
  std::unique_ptr<BrowserTabsModelProvider> browser_tabs_model_provider_;
  std::unique_ptr<BrowserTabsModelController> browser_tabs_model_controller_;
  std::unique_ptr<MultideviceSetupStateUpdater>
      multidevice_setup_state_updater_;
  std::unique_ptr<InvalidConnectionDisconnector>
      invalid_connection_disconnector_;
  std::unique_ptr<CameraRollManager> camera_roll_manager_;
  std::unique_ptr<FeatureSetupResponseProcessor>
      feature_setup_response_processor_;
  std::unique_ptr<PingManager> ping_manager_;
  raw_ptr<eche_app::EcheConnectionStatusHandler>
      eche_connection_status_handler_ = nullptr;
  raw_ptr<eche_app::SystemInfoProvider> system_info_provider_ = nullptr;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_IMPL_H_

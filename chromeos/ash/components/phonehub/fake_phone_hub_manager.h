// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/app_stream_manager.h"
#include "chromeos/ash/components/phonehub/fake_browser_tabs_model_provider.h"
#include "chromeos/ash/components/phonehub/fake_camera_roll_manager.h"
#include "chromeos/ash/components/phonehub/fake_connection_scheduler.h"
#include "chromeos/ash/components/phonehub/fake_do_not_disturb_controller.h"
#include "chromeos/ash/components/phonehub/fake_feature_status_provider.h"
#include "chromeos/ash/components/phonehub/fake_find_my_device_controller.h"
#include "chromeos/ash/components/phonehub/fake_icon_decoder.h"
#include "chromeos/ash/components/phonehub/fake_multidevice_feature_access_manager.h"
#include "chromeos/ash/components/phonehub/fake_notification_interaction_handler.h"
#include "chromeos/ash/components/phonehub/fake_notification_manager.h"
#include "chromeos/ash/components/phonehub/fake_onboarding_ui_tracker.h"
#include "chromeos/ash/components/phonehub/fake_ping_manager.h"
#include "chromeos/ash/components/phonehub/fake_recent_apps_interaction_handler.h"
#include "chromeos/ash/components/phonehub/fake_screen_lock_manager.h"
#include "chromeos/ash/components/phonehub/fake_tether_controller.h"
#include "chromeos/ash/components/phonehub/fake_user_action_recorder.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/phone_hub_ui_readiness_recorder.h"

namespace ash {
namespace phonehub {

// This class initializes fake versions of the core business logic of Phone Hub.
class FakePhoneHubManager : public PhoneHubManager {
 public:
  FakePhoneHubManager();
  ~FakePhoneHubManager() override;

  FakeDoNotDisturbController* fake_do_not_disturb_controller() {
    return &fake_do_not_disturb_controller_;
  }

  FakeFeatureStatusProvider* fake_feature_status_provider() {
    return &fake_feature_status_provider_;
  }

  FakeFindMyDeviceController* fake_find_my_device_controller() {
    return &fake_find_my_device_controller_;
  }

  FakeMultideviceFeatureAccessManager*
  fake_multidevice_feature_access_manager() {
    return &fake_multidevice_feature_access_manager_;
  }

  FakeNotificationInteractionHandler* fake_notification_interaction_handler() {
    return &fake_notification_interaction_handler_;
  }

  FakeNotificationManager* fake_notification_manager() {
    return &fake_notification_manager_;
  }

  FakeOnboardingUiTracker* fake_onboarding_ui_tracker() {
    return &fake_onboarding_ui_tracker_;
  }

  AppStreamLauncherDataModel* fake_app_stream_launcher_data_model() {
    return &app_stream_launcher_data_model_;
  }

  FakeRecentAppsInteractionHandler* fake_recent_apps_interaction_handler() {
    return &fake_recent_apps_interaction_handler_;
  }

  FakeScreenLockManager* fake_screen_lock_manager() {
    return &fake_screen_lock_manager_;
  }

  MutablePhoneModel* mutable_phone_model() { return &mutable_phone_model_; }

  FakeTetherController* fake_tether_controller() {
    return &fake_tether_controller_;
  }

  FakeConnectionScheduler* fake_connection_scheduler() {
    return &fake_connection_scheduler_;
  }

  FakeUserActionRecorder* fake_user_action_recorder() {
    return &fake_user_action_recorder_;
  }

  FakeBrowserTabsModelProvider* fake_browser_tabs_model_provider() {
    return &fake_browser_tabs_model_provider_;
  }

  FakeCameraRollManager* fake_camera_roll_manager() {
    return &fake_camera_roll_manager_;
  }

  FakePingManager* fake_ping_manager() { return &fake_ping_manager_; }

  FakeIconDecoder* fake_icon_decoder() { return &fake_icon_decoder_; }

  void set_host_last_seen_timestamp(std::optional<base::Time> timestamp) {
    host_last_seen_timestamp_ = timestamp;
  }

  void set_eche_connection_handler(
      eche_app::EcheConnectionStatusHandler* handler) {
    eche_connection_status_handler_ = handler;
  }

 private:
  // PhoneHubManager:
  BrowserTabsModelProvider* GetBrowserTabsModelProvider() override;
  CameraRollManager* GetCameraRollManager() override;
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
  FakePingManager* GetPingManager() override;
  RecentAppsInteractionHandler* GetRecentAppsInteractionHandler() override;
  ScreenLockManager* GetScreenLockManager() override;
  TetherController* GetTetherController() override;
  ConnectionScheduler* GetConnectionScheduler() override;
  UserActionRecorder* GetUserActionRecorder() override;
  void GetHostLastSeenTimestamp(
      base::OnceCallback<void(std::optional<base::Time>)> callback) override;
  IconDecoder* GetIconDecoder() override;
  AppStreamManager* GetAppStreamManager() override;
  PhoneHubUiReadinessRecorder* GetPhoneHubUiReadinessRecorder() override;
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

  FakeDoNotDisturbController fake_do_not_disturb_controller_;
  FakeFeatureStatusProvider fake_feature_status_provider_;
  FakeFindMyDeviceController fake_find_my_device_controller_;
  FakeMultideviceFeatureAccessManager fake_multidevice_feature_access_manager_;
  FakeNotificationInteractionHandler fake_notification_interaction_handler_;
  FakeNotificationManager fake_notification_manager_;
  FakeOnboardingUiTracker fake_onboarding_ui_tracker_;
  AppStreamLauncherDataModel app_stream_launcher_data_model_;
  MutablePhoneModel mutable_phone_model_;
  FakeRecentAppsInteractionHandler fake_recent_apps_interaction_handler_;
  FakeScreenLockManager fake_screen_lock_manager_;
  FakeTetherController fake_tether_controller_;
  FakeConnectionScheduler fake_connection_scheduler_;
  FakeUserActionRecorder fake_user_action_recorder_;
  FakeBrowserTabsModelProvider fake_browser_tabs_model_provider_;
  FakeCameraRollManager fake_camera_roll_manager_;
  FakePingManager fake_ping_manager_;
  FakeIconDecoder fake_icon_decoder_;
  AppStreamManager app_stream_manager_;
  raw_ptr<PhoneHubUiReadinessRecorder> phone_hub_ui_readiness_recorder_ =
      nullptr;
  raw_ptr<PhoneHubStructuredMetricsLogger>
      phone_hub_structured_metrics_logger_ = nullptr;
  raw_ptr<eche_app::EcheConnectionStatusHandler, DanglingUntriaged>
      eche_connection_status_handler_ = nullptr;
  raw_ptr<eche_app::SystemInfoProvider, DanglingUntriaged>
      system_info_provider_ = nullptr;
  std::optional<base::Time> host_last_seen_timestamp_ = std::nullopt;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_FAKE_PHONE_HUB_MANAGER_H_

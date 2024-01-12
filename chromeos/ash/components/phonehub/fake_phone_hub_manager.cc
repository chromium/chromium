// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"

#include "ash/constants/ash_features.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"

namespace ash {
namespace phonehub {

FakePhoneHubManager::FakePhoneHubManager() = default;

FakePhoneHubManager::~FakePhoneHubManager() = default;

BrowserTabsModelProvider* FakePhoneHubManager::GetBrowserTabsModelProvider() {
  return &fake_browser_tabs_model_provider_;
}

CameraRollManager* FakePhoneHubManager::GetCameraRollManager() {
  return features::IsPhoneHubCameraRollEnabled() ? &fake_camera_roll_manager_
                                                 : nullptr;
}

DoNotDisturbController* FakePhoneHubManager::GetDoNotDisturbController() {
  return &fake_do_not_disturb_controller_;
}

FeatureStatusProvider* FakePhoneHubManager::GetFeatureStatusProvider() {
  return &fake_feature_status_provider_;
}

FindMyDeviceController* FakePhoneHubManager::GetFindMyDeviceController() {
  return &fake_find_my_device_controller_;
}

MultideviceFeatureAccessManager*
FakePhoneHubManager::GetMultideviceFeatureAccessManager() {
  return &fake_multidevice_feature_access_manager_;
}

NotificationInteractionHandler*
FakePhoneHubManager::GetNotificationInteractionHandler() {
  return features::IsEcheSWAEnabled() ? &fake_notification_interaction_handler_
                                      : nullptr;
}

NotificationManager* FakePhoneHubManager::GetNotificationManager() {
  return &fake_notification_manager_;
}

OnboardingUiTracker* FakePhoneHubManager::GetOnboardingUiTracker() {
  return &fake_onboarding_ui_tracker_;
}

AppStreamLauncherDataModel*
FakePhoneHubManager::GetAppStreamLauncherDataModel() {
  return &app_stream_launcher_data_model_;
}

PhoneModel* FakePhoneHubManager::GetPhoneModel() {
  return &mutable_phone_model_;
}

RecentAppsInteractionHandler*
FakePhoneHubManager::GetRecentAppsInteractionHandler() {
  return features::IsEcheSWAEnabled() ? &fake_recent_apps_interaction_handler_
                                      : nullptr;
}

ScreenLockManager* FakePhoneHubManager::GetScreenLockManager() {
  return features::IsEcheSWAEnabled() ? &fake_screen_lock_manager_ : nullptr;
}

TetherController* FakePhoneHubManager::GetTetherController() {
  return &fake_tether_controller_;
}

ConnectionScheduler* FakePhoneHubManager::GetConnectionScheduler() {
  return &fake_connection_scheduler_;
}

UserActionRecorder* FakePhoneHubManager::GetUserActionRecorder() {
  return &fake_user_action_recorder_;
}

FakePingManager* FakePhoneHubManager::GetPingManager() {
  return features::IsPhoneHubPingOnBubbleOpenEnabled() ? &fake_ping_manager_
                                                       : nullptr;
}

void FakePhoneHubManager::GetHostLastSeenTimestamp(
    base::OnceCallback<void(std::optional<base::Time>)> callback) {
  std::move(callback).Run(host_last_seen_timestamp_);
}

IconDecoder* FakePhoneHubManager::GetIconDecoder() {
  return &fake_icon_decoder_;
}

AppStreamManager* FakePhoneHubManager::GetAppStreamManager() {
  return &app_stream_manager_;
}

PhoneHubUiReadinessRecorder*
FakePhoneHubManager::GetPhoneHubUiReadinessRecorder() {
  return phone_hub_ui_readiness_recorder_;
}

eche_app::EcheConnectionStatusHandler*
FakePhoneHubManager::GetEcheConnectionStatusHandler() {
  return eche_connection_status_handler_;
}

void FakePhoneHubManager::SetEcheConnectionStatusHandler(
    eche_app::EcheConnectionStatusHandler* eche_connection_status_handler) {
  eche_connection_status_handler_ = eche_connection_status_handler;
}

void FakePhoneHubManager::SetSystemInfoProvider(
    eche_app::SystemInfoProvider* system_info_provider) {
  system_info_provider_ = system_info_provider;
}

eche_app::SystemInfoProvider* FakePhoneHubManager::GetSystemInfoProvider() {
  return system_info_provider_;
}

PhoneHubStructuredMetricsLogger*
FakePhoneHubManager::GetPhoneHubStructuredMetricsLogger() {
  return phone_hub_structured_metrics_logger_;
}

}  // namespace phonehub
}  // namespace ash

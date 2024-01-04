// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/app_stream_manager.h"

namespace ash {

namespace eche_app {
class EcheConnectionStatusHandler;
class SystemInfoProvider;
}

namespace phonehub {

class BrowserTabsModelProvider;
class CameraRollManager;
class ConnectionScheduler;
class DoNotDisturbController;
class FeatureStatusProvider;
class FindMyDeviceController;
class MultideviceFeatureAccessManager;
class NotificationInteractionHandler;
class NotificationManager;
class OnboardingUiTracker;
class PhoneModel;
class PingManager;
class RecentAppsInteractionHandler;
class ScreenLockManager;
class TetherController;
class UserActionRecorder;
class IconDecoder;
class AppStreamManager;
class PhoneHubUiReadinessRecorder;
class PhoneHubStructuredMetricsLogger;

// Responsible for the core logic of the Phone Hub feature and exposes
// interfaces via its public API. This class is intended to be a singleton.
class PhoneHubManager {
 public:
  virtual ~PhoneHubManager() = default;

  PhoneHubManager(const PhoneHubManager&) = delete;
  PhoneHubManager& operator=(const PhoneHubManager&) = delete;

  // Getters for sub-elements.
  virtual BrowserTabsModelProvider* GetBrowserTabsModelProvider() = 0;
  virtual CameraRollManager* GetCameraRollManager() = 0;
  virtual ConnectionScheduler* GetConnectionScheduler() = 0;
  virtual DoNotDisturbController* GetDoNotDisturbController() = 0;
  virtual FeatureStatusProvider* GetFeatureStatusProvider() = 0;
  virtual FindMyDeviceController* GetFindMyDeviceController() = 0;
  virtual MultideviceFeatureAccessManager*
  GetMultideviceFeatureAccessManager() = 0;
  virtual NotificationInteractionHandler*
  GetNotificationInteractionHandler() = 0;
  virtual NotificationManager* GetNotificationManager() = 0;
  virtual OnboardingUiTracker* GetOnboardingUiTracker() = 0;
  virtual AppStreamLauncherDataModel* GetAppStreamLauncherDataModel() = 0;
  virtual PhoneModel* GetPhoneModel() = 0;
  virtual PingManager* GetPingManager() = 0;
  virtual RecentAppsInteractionHandler* GetRecentAppsInteractionHandler() = 0;
  virtual ScreenLockManager* GetScreenLockManager() = 0;
  virtual TetherController* GetTetherController() = 0;
  virtual UserActionRecorder* GetUserActionRecorder() = 0;
  virtual IconDecoder* GetIconDecoder() = 0;
  virtual AppStreamManager* GetAppStreamManager() = 0;
  virtual eche_app::EcheConnectionStatusHandler*
  GetEcheConnectionStatusHandler() = 0;
  virtual PhoneHubUiReadinessRecorder* GetPhoneHubUiReadinessRecorder() = 0;
  virtual void SetEcheConnectionStatusHandler(
      eche_app::EcheConnectionStatusHandler*
          eche_connection_status_handler) = 0;
  virtual void SetSystemInfoProvider(
      eche_app::SystemInfoProvider* system_info_provider) = 0;
  virtual eche_app::SystemInfoProvider* GetSystemInfoProvider() = 0;
  virtual PhoneHubStructuredMetricsLogger*
  GetPhoneHubStructuredMetricsLogger() = 0;

  // Retrieves the timestamp of the last successful discovery for active host,
  // or nullopt if it hasn't been seen in the current Chrome session.
  virtual void GetHostLastSeenTimestamp(
      base::OnceCallback<void(std::optional<base::Time>)> callback) = 0;

 protected:
  PhoneHubManager() = default;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_HUB_MANAGER_H_

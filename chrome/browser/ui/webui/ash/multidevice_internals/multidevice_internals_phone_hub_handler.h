// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_PHONE_HUB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_PHONE_HUB_HANDLER_H_

#include "base/scoped_observation.h"
#include "chromeos/ash/components/phonehub/camera_roll_manager.h"
#include "chromeos/ash/components/phonehub/do_not_disturb_controller.h"
#include "chromeos/ash/components/phonehub/find_my_device_controller.h"
#include "chromeos/ash/components/phonehub/notification_manager.h"
#include "chromeos/ash/components/phonehub/onboarding_ui_tracker.h"
#include "chromeos/ash/components/phonehub/tether_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {

namespace phonehub {
class FakePhoneHubManager;
}

namespace multidevice {

// WebUIMessageHandler for chrome://multidevice-internals PhoneHub section.
class MultidevicePhoneHubHandler
    : public content::WebUIMessageHandler,
      public phonehub::NotificationManager::Observer,
      public phonehub::DoNotDisturbController::Observer,
      public phonehub::FindMyDeviceController::Observer,
      public phonehub::TetherController::Observer,
      public phonehub::OnboardingUiTracker::Observer,
      public phonehub::CameraRollManager::Observer {
 public:
  MultidevicePhoneHubHandler();
  MultidevicePhoneHubHandler(const MultidevicePhoneHubHandler&) = delete;
  MultidevicePhoneHubHandler& operator=(const MultidevicePhoneHubHandler&) =
      delete;
  ~MultidevicePhoneHubHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override;

 private:
  // NotificationManager::Observer
  void OnNotificationsRemoved(
      const base::flat_set<int64_t>& notification_ids) override;

  // DoNotDisturbController::Observer
  void OnDndStateChanged() override;

  // FindMyDeviceController::Observer
  void OnPhoneRingingStateChanged() override;

  // TetherController::Observer
  void OnTetherStatusChanged() override;

  // OnboardingUiTracker::Observer
  void OnShouldShowOnboardingUiChanged() override;

  // CameraRollManager::Observer:
  void OnCameraRollViewUiStateUpdated() override;

  void EnableRealPhoneHubManager();
  void EnableFakePhoneHubManager();
  void HandleEnableFakePhoneHubManager(const base::Value::List& args);
  void HandleSetFeatureStatus(const base::Value::List& args);
  void HandleSetShowOnboardingFlow(const base::Value::List& args);
  void HandleSetFakePhoneName(const base::Value::List& args);
  void HandleSetFakePhoneStatus(const base::Value::List& args);
  void HandleSetBrowserTabs(const base::Value::List& args);
  void HandleSetNotification(const base::Value::List& args);
  void HandleRemoveNotification(const base::Value::List& args);
  void HandleEnableDnd(const base::Value::List& args);
  void HandleSetFindMyDeviceStatus(const base::Value::List& args);
  void HandleSetTetherStatus(const base::Value::List& args);
  void HandleResetShouldShowOnboardingUi(const base::Value::List& args);
  void HandleResetHasMultideviceFeatureSetupUiBeenDismissed(
      const base::Value::List& args);
  void HandleSetFakeCameraRoll(const base::Value::List& args);

  void AddObservers();
  void RemoveObservers();

  std::unique_ptr<phonehub::FakePhoneHubManager> fake_phone_hub_manager_;
  base::ScopedObservation<phonehub::NotificationManager,
                          phonehub::NotificationManager::Observer>
      notification_manager_observation_{this};
  base::ScopedObservation<phonehub::DoNotDisturbController,
                          phonehub::DoNotDisturbController::Observer>
      do_not_disturb_controller_observation_{this};
  base::ScopedObservation<phonehub::FindMyDeviceController,
                          phonehub::FindMyDeviceController::Observer>
      find_my_device_controller_observation_{this};
  base::ScopedObservation<phonehub::TetherController,
                          phonehub::TetherController::Observer>
      tether_controller_observation_{this};
  base::ScopedObservation<phonehub::OnboardingUiTracker,
                          phonehub::OnboardingUiTracker::Observer>
      onboarding_ui_tracker_observation_{this};
  base::ScopedObservation<phonehub::CameraRollManager,
                          phonehub::CameraRollManager::Observer>
      camera_roll_manager_observation_{this};
};

}  // namespace multidevice
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_PHONE_HUB_HANDLER_H_

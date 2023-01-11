// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_DEVICE_ACTIONS_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_DEVICE_ACTIONS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

namespace ash::assistant {

// Subscribes to App list events.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) AppListEventSubscriber
    : public base::CheckedObserver {
 public:
  // Called when the android app list changed.
  virtual void OnAndroidAppListRefreshed(
      const std::vector<AndroidAppInfo>& apps_info) = 0;
};

// Main interface for |ash::assistant::Service| to execute device related
// actions.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) DeviceActions {
 public:
  DeviceActions();
  DeviceActions(const DeviceActions&) = delete;
  DeviceActions& operator=(const DeviceActions&) = delete;
  virtual ~DeviceActions();

  static DeviceActions* Get();

  // Enables or disables WiFi.
  virtual void SetWifiEnabled(bool enabled) = 0;

  // Enables or disables Bluetooth.
  virtual void SetBluetoothEnabled(bool enabled) = 0;

  // Gets the current screen brightness level (0-1.0).
  // The level is set to 0 in the event of an error.
  using GetScreenBrightnessLevelCallback =
      base::OnceCallback<void(bool success, double level)>;
  virtual void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) = 0;

  // Sets the screen brightness level (0-1.0).  If |gradual| is true, the
  // transition will be animated.
  virtual void SetScreenBrightnessLevel(double level, bool gradual) = 0;

  // Enables or disables Night Light.
  virtual void SetNightLightEnabled(bool enabled) = 0;

  // Enables or disables Switch Access.
  virtual void SetSwitchAccessEnabled(bool enabled) = 0;

  // Open the Android app if the app is available. Returns true if app is
  // successfully opened, false otherwise.
  virtual bool OpenAndroidApp(const AndroidAppInfo& app_info) = 0;

  // Get the status of the Android app.
  virtual AppStatus GetAndroidAppStatus(const AndroidAppInfo& app_info) = 0;

  // Launch Android intent. The intent is encoded as a URI string.
  // See Intent.toUri().
  virtual void LaunchAndroidIntent(const std::string& intent) = 0;

  // Register App list event subscriber. The subscriber will be immediately
  // called with the current App list, and then for every change.
  virtual void AddAndFireAppListEventSubscriber(
      AppListEventSubscriber* subscriber) = 0;
  virtual void RemoveAppListEventSubscriber(
      AppListEventSubscriber* subscriber) = 0;
};

}  // namespace ash::assistant

// TODO(b/258750971): remove when internal assistant codes are migrated to
// namespace ash.
namespace chromeos::assistant {
using ::ash::assistant::AppListEventSubscriber;
using ::ash::assistant::DeviceActions;
}  // namespace chromeos::assistant

namespace base {

template <>
struct ScopedObservationTraits<ash::assistant::DeviceActions,
                               ash::assistant::AppListEventSubscriber> {
  static void AddObserver(ash::assistant::DeviceActions* source,
                          ash::assistant::AppListEventSubscriber* observer) {
    source->AddAndFireAppListEventSubscriber(observer);
  }
  static void RemoveObserver(ash::assistant::DeviceActions* source,
                             ash::assistant::AppListEventSubscriber* observer) {
    source->RemoveAppListEventSubscriber(observer);
  }
};

}  // namespace base

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_DEVICE_ACTIONS_H_

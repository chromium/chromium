// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/android_intent_helper.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/assistant/device_actions_delegate.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/device_actions.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class DeviceActions : public ash::AndroidIntentHelper,
                      public chromeos::assistant::DeviceActions,
                      public ArcAppListPrefs::Observer {
 public:
  explicit DeviceActions(std::unique_ptr<DeviceActionsDelegate> delegate);
  ~DeviceActions() override;

  // chromeos::assistant::DeviceActions overrides:
  void SetWifiEnabled(bool enabled) override;
  void SetBluetoothEnabled(bool enabled) override;
  void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) override;
  void SetScreenBrightnessLevel(double level, bool gradual) override;
  void SetNightLightEnabled(bool enabled) override;
  void SetSwitchAccessEnabled(bool enabled) override;
  bool OpenAndroidApp(
      const chromeos::assistant::AndroidAppInfo& app_info) override;
  chromeos::assistant::AppStatus GetAndroidAppStatus(
      const chromeos::assistant::AndroidAppInfo& app_info) override;
  void LaunchAndroidIntent(const std::string& intent) override;
  void AddAndFireAppListEventSubscriber(
      chromeos::assistant::AppListEventSubscriber* subscriber) override;
  void RemoveAppListEventSubscriber(
      chromeos::assistant::AppListEventSubscriber* subscriber) override;

  // ash::AndroidIntentHelper overrides:
  base::Optional<std::string> GetAndroidAppLaunchIntent(
      const chromeos::assistant::AndroidAppInfo& app_info) override;

 private:
  // ArcAppListPrefs::Observer overrides.
  void OnPackageListInitialRefreshed() override;
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& id) override;

  std::unique_ptr<DeviceActionsDelegate> delegate_;

  ScopedObserver<ArcAppListPrefs, ArcAppListPrefs::Observer>
      scoped_prefs_observer_{this};
  base::ObserverList<chromeos::assistant::AppListEventSubscriber>
      app_list_subscribers_;
  DISALLOW_COPY_AND_ASSIGN(DeviceActions);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_H_

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_ANDROID_APPS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_ANDROID_APPS_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace ash::settings {

class AndroidAppsHandler : public ::settings::SettingsPageUIHandler,
                           public ArcAppListPrefs::Observer,
                           public arc::ArcSessionManagerObserver {
 public:
  explicit AndroidAppsHandler(Profile* profile,
                              apps::AppServiceProxy* app_service_proxy);

  AndroidAppsHandler(const AndroidAppsHandler&) = delete;
  AndroidAppsHandler& operator=(const AndroidAppsHandler&) = delete;

  ~AndroidAppsHandler() override;

  // SettingsPageUIHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ArcAppListPrefs::Observer
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& app_id) override;

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

 private:
  base::Value::Dict BuildAndroidAppsInfo();
  void HandleRequestAndroidAppsInfo(const base::Value::List& args);
  void HandleAppChanged(const std::string& app_id);
  void SendAndroidAppsInfo();
  void ShowAndroidAppsSettings(const base::Value::List& args);
  int64_t GetDisplayIdForCurrentProfile();

  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_prefs_observation_{this};
  base::ScopedObservation<arc::ArcSessionManager,
                          arc::ArcSessionManagerObserver>
      arc_session_manager_observation_{this};
  raw_ptr<Profile> profile_;  // unowned
  raw_ptr<apps::AppServiceProxy> app_service_proxy_;
  base::WeakPtrFactory<AndroidAppsHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_ANDROID_APPS_HANDLER_H_

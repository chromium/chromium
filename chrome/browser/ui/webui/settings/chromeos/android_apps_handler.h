// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ANDROID_APPS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ANDROID_APPS_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace base {
class DictionaryValue;
}

namespace chromeos {
namespace settings {

class AndroidAppsHandler : public ::settings::SettingsPageUIHandler,
                           public ArcAppListPrefs::Observer,
                           public arc::ArcSessionManager::Observer {
 public:
  explicit AndroidAppsHandler(Profile* profile);
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

  // arc::ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

 private:
  std::unique_ptr<base::DictionaryValue> BuildAndroidAppsInfo();
  void HandleRequestAndroidAppsInfo(const base::ListValue* args);
  void HandleAppChanged(const std::string& app_id);
  void SendAndroidAppsInfo();
  void ShowAndroidAppsSettings(const base::ListValue* args);
  void ShowAndroidManageAppLinks(const base::ListValue* args);
  int64_t GetDisplayIdForCurrentProfile();

  ScopedObserver<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_prefs_observer_;
  ScopedObserver<arc::ArcSessionManager, arc::ArcSessionManager::Observer>
      arc_session_manager_observer_;
  Profile* profile_;  // unowned
  base::WeakPtrFactory<AndroidAppsHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AndroidAppsHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_ANDROID_APPS_HANDLER_H_

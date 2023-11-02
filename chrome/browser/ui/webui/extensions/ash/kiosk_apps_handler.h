// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_ASH_KIOSK_APPS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_ASH_KIOSK_APPS_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash {

class OwnerSettingsServiceAsh;

class KioskAppsHandler : public content::WebUIMessageHandler,
                         public KioskAppManagerObserver {
 public:
  explicit KioskAppsHandler(OwnerSettingsServiceAsh* service);

  KioskAppsHandler(const KioskAppsHandler&) = delete;
  KioskAppsHandler& operator=(const KioskAppsHandler&) = delete;

  ~KioskAppsHandler() override;

  // content::WebUIMessageHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // KioskAppManagerObserver overrides:
  void OnKioskAppDataChanged(const std::string& app_id) override;
  void OnKioskAppDataLoadFailure(const std::string& app_id) override;
  void OnKioskExtensionLoadedInCache(const std::string& app_id) override;
  void OnKioskExtensionDownloadFailed(const std::string& app_id) override;

  void OnKioskAppsSettingsChanged() override;

 private:
  // Get all kiosk apps and settings.
  base::Value::Dict GetSettingsDictionary();

  // JS callbacks.
  void HandleInitializeKioskAppSettings(const base::Value::List& args);
  void HandleGetKioskAppSettings(const base::Value::List& args);
  void HandleAddKioskApp(const base::Value::List& args);
  void HandleRemoveKioskApp(const base::Value::List& args);
  void HandleEnableKioskAutoLaunch(const base::Value::List& args);
  void HandleDisableKioskAutoLaunch(const base::Value::List& args);
  void HandleSetDisableBailoutShortcut(const base::Value::List& args);

  void UpdateApp(const std::string& app_id);
  void ShowError(const std::string& app_id);

  // Callback for KioskAppManager::GetConsumerKioskModeStatus().
  void OnGetConsumerKioskAutoLaunchStatus(
      const std::string& callback_id,
      KioskAppManager::ConsumerKioskAutoLaunchStatus status);

  KioskAppManager* kiosk_app_manager_;  // not owned.
  bool initialized_;
  bool is_kiosk_enabled_;
  bool is_auto_launch_enabled_;
  OwnerSettingsServiceAsh* const owner_settings_service_;  // not owned
  base::WeakPtrFactory<KioskAppsHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_ASH_KIOSK_APPS_HANDLER_H_

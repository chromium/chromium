// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/kiosk_autolaunch_screen_handler.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

KioskAutolaunchScreenHandler::KioskAutolaunchScreenHandler()
    : BaseScreenHandler(kScreenId) {
  KioskChromeAppManager::Get()->AddObserver(this);
}

KioskAutolaunchScreenHandler::~KioskAutolaunchScreenHandler() {
  KioskChromeAppManager::Get()->RemoveObserver(this);
}

void KioskAutolaunchScreenHandler::Show() {
  UpdateKioskApp();
  ShowInWebUI();
}

void KioskAutolaunchScreenHandler::UpdateKioskApp() {
  if (!is_visible_)
    return;

  KioskChromeAppManager* manager = KioskChromeAppManager::Get();
  KioskChromeAppManager::App app;
  std::string app_id = manager->GetAutoLaunchApp();
  if (app_id.empty() ||
      manager->IsAutoLaunchEnabled() ||
      !manager->GetApp(app_id, &app)) {
    return;
  }

  base::Value::Dict app_info;
  app_info.Set("appName", app.name);

  std::string icon_url("chrome://theme/IDR_APP_DEFAULT_ICON");
  if (!app.icon.isNull())
    icon_url = webui::GetBitmapDataUrl(*app.icon.bitmap());

  app_info.Set("appIconUrl", icon_url);
  CallExternalAPI("updateApp", std::move(app_info));
}

void KioskAutolaunchScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("autolaunchTitle", IDS_AUTOSTART_WARNING_TITLE);
  builder->Add("autolaunchWarningTitle", IDS_AUTOSTART_WARNING_TITLE);
  builder->Add("autolaunchWarning", IDS_KIOSK_AUTOSTART_SCREEN_WARNING_MSG);
  builder->Add("autolaunchConfirmButton", IDS_KIOSK_AUTOSTART_CONFIRM);
  builder->Add("autolaunchCancelButton", IDS_CANCEL);
}

void KioskAutolaunchScreenHandler::HandleOnCancel() {
  KioskChromeAppManager::Get()->RemoveObserver(this);
  KioskChromeAppManager::Get()->SetEnableAutoLaunch(false);
}

void KioskAutolaunchScreenHandler::HandleOnConfirm() {
  KioskChromeAppManager::Get()->RemoveObserver(this);
  KioskChromeAppManager::Get()->SetEnableAutoLaunch(true);
}

void KioskAutolaunchScreenHandler::DeclareJSCallbacks() {
  AddCallback("autolaunchVisible",
              &KioskAutolaunchScreenHandler::HandleOnVisible);
}

void KioskAutolaunchScreenHandler::HandleOnVisible() {
  if (is_visible_)
    return;

  is_visible_ = true;
  UpdateKioskApp();
}

void KioskAutolaunchScreenHandler::OnKioskAppsSettingsChanged() {
  UpdateKioskApp();
}

void KioskAutolaunchScreenHandler::OnKioskAppDataChanged(
    const std::string& app_id) {
  UpdateKioskApp();
}

}  // namespace ash

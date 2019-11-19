// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {

constexpr StaticOobeScreenId KioskAutolaunchScreenView::kScreenId;

KioskAutolaunchScreenHandler::KioskAutolaunchScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  KioskAppManager::Get()->AddObserver(this);
}

KioskAutolaunchScreenHandler::~KioskAutolaunchScreenHandler() {
  if (delegate_)
    delegate_->OnViewDestroyed(this);

  KioskAppManager::Get()->RemoveObserver(this);
}

void KioskAutolaunchScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  UpdateKioskApp();
  ShowScreen(kScreenId);
}

void KioskAutolaunchScreenHandler::SetDelegate(
    KioskAutolaunchScreen* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void KioskAutolaunchScreenHandler::UpdateKioskApp() {
  if (!is_visible_)
    return;

  KioskAppManager* manager = KioskAppManager::Get();
  KioskAppManager::App app;
  std::string app_id = manager->GetAutoLaunchApp();
  if (app_id.empty() ||
      manager->IsAutoLaunchEnabled() ||
      !manager->GetApp(app_id, &app)) {
    return;
  }

  base::DictionaryValue app_info;
  app_info.SetString("appName", app.name);

  std::string icon_url("chrome://theme/IDR_APP_DEFAULT_ICON");
  if (!app.icon.isNull())
    icon_url = webui::GetBitmapDataUrl(*app.icon.bitmap());

  app_info.SetString("appIconUrl", icon_url);
  CallJS("login.AutolaunchScreen.updateApp", app_info);
}

void KioskAutolaunchScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("autolaunchTitle", IDS_AUTOSTART_WARNING_TITLE);
  builder->Add("autolaunchWarningTitle", IDS_AUTOSTART_WARNING_TITLE);
  builder->Add("autolaunchWarning", IDS_KIOSK_AUTOSTART_SCREEN_WARNING_MSG);
  builder->Add("autolaunchConfirmButton", IDS_KIOSK_AUTOSTART_CONFIRM);
  builder->Add("autolaunchCancelButton", IDS_CANCEL);
}

void KioskAutolaunchScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void KioskAutolaunchScreenHandler::RegisterMessages() {
  AddCallback("autolaunchVisible",
              &KioskAutolaunchScreenHandler::HandleOnVisible);
  AddCallback("autolaunchOnCancel",
              &KioskAutolaunchScreenHandler::HandleOnCancel);
  AddCallback("autolaunchOnConfirm",
              &KioskAutolaunchScreenHandler::HandleOnConfirm);
}

void KioskAutolaunchScreenHandler::HandleOnCancel() {
  KioskAppManager::Get()->RemoveObserver(this);
  KioskAppManager::Get()->SetEnableAutoLaunch(false);
  if (delegate_)
    delegate_->OnExit(false);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void KioskAutolaunchScreenHandler::HandleOnConfirm() {
  KioskAppManager::Get()->RemoveObserver(this);
  KioskAppManager::Get()->SetEnableAutoLaunch(true);
  if (delegate_)
    delegate_->OnExit(true);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_COMPLETED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void KioskAutolaunchScreenHandler::HandleOnVisible() {
  if (is_visible_)
    return;

  is_visible_ = true;
  UpdateKioskApp();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_AUTOLAUNCH_WARNING_VISIBLE,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

void KioskAutolaunchScreenHandler::OnKioskAppsSettingsChanged() {
  UpdateKioskApp();
}

void KioskAutolaunchScreenHandler::OnKioskAppDataChanged(
    const std::string& app_id) {
  UpdateKioskApp();
}

}  // namespace chromeos

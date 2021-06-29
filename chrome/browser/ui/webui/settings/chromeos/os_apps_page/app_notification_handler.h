// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_APPS_PAGE_APP_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_APPS_PAGE_APP_NOTIFICATION_HANDLER_H_

#include "ash/public/cpp/message_center_ash.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace chromeos {
namespace settings {

class AppNotificationHandler : public ::settings::SettingsPageUIHandler,
                               public ash::MessageCenterAsh::Observer {
 public:
  AppNotificationHandler();
  ~AppNotificationHandler() override;

  // SettingsPageUIHandler
  void RegisterMessages() override {}
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  friend class AppNotificationHandlerTest;

  // MessageCenterAsh::Observer Override
  void OnQuietModeChanged(bool in_quiet_mode) override;

  void SetQuietMode(bool in_quiet_mode);

  bool in_quiet_mode_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_APPS_PAGE_APP_NOTIFICATION_HANDLER_H_

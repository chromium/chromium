// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_APPS_PAGE_APP_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_APPS_PAGE_APP_NOTIFICATION_HANDLER_H_

#include "ash/public/cpp/message_center_ash.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_apps_page/mojom/app_notification_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace settings {

class AppNotificationHandler
    : public app_notification::mojom::AppNotificationsHandler,
      public ash::MessageCenterAsh::Observer {
 public:
  AppNotificationHandler();
  ~AppNotificationHandler() override;

  // app_notification::mojom::AppNotificationHandler:
  void AddObserver(
      mojo::PendingRemote<app_notification::mojom::AppNotificationsObserver>
          observer) override;

  void BindInterface(
      mojo::PendingReceiver<app_notification::mojom::AppNotificationsHandler>
          receiver);

 private:
  friend class AppNotificationHandlerTest;

  // MessageCenterAsh::Observer override:
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // settings::mojom::AppNotificationHandler:
  void SetQuietMode(bool in_quiet_mode) override;

  bool in_quiet_mode_;

  mojo::RemoteSet<app_notification::mojom::AppNotificationsObserver>
      observer_list_;

  mojo::Receiver<app_notification::mojom::AppNotificationsHandler> receiver_{
      this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_APPS_PAGE_APP_NOTIFICATION_HANDLER_H_

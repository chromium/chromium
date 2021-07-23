// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_apps_page/app_notification_handler.h"
#include "ash/public/cpp/message_center_ash.h"
#include "base/logging.h"

namespace chromeos {
namespace settings {

using app_notification::mojom::AppNotificationsHandler;
using app_notification::mojom::AppNotificationsObserver;

AppNotificationHandler ::AppNotificationHandler() {
  if (ash::MessageCenterAsh::Get()) {
    ash::MessageCenterAsh::Get()->AddObserver(this);
  }
}

AppNotificationHandler::~AppNotificationHandler() {
  if (ash::MessageCenterAsh::Get()) {
    ash::MessageCenterAsh::Get()->RemoveObserver(this);
  }
}

void AppNotificationHandler::AddObserver(
    mojo::PendingRemote<app_notification::mojom::AppNotificationsObserver>
        observer) {
  observer_list_.Add(std::move(observer));
}

void AppNotificationHandler::BindInterface(
    mojo::PendingReceiver<app_notification::mojom::AppNotificationsHandler>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void AppNotificationHandler ::OnQuietModeChanged(bool in_quiet_mode) {
  in_quiet_mode_ = in_quiet_mode;
  for (const auto& observer : observer_list_) {
    observer->OnQuietModeChanged(in_quiet_mode);
  }
}

void AppNotificationHandler ::SetQuietMode(bool in_quiet_mode) {
  ash::MessageCenterAsh::Get()->SetQuietMode(in_quiet_mode);
}

}  // namespace settings
}  // namespace chromeos

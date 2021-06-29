// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_apps_page/app_notification_handler.h"
#include "ash/public/cpp/message_center_ash.h"
#include "base/logging.h"

namespace chromeos {
namespace settings {

using apps_notification::mojom::AppNotificationsHandler;

AppNotificationHandler ::AppNotificationHandler() {
  ash::MessageCenterAsh::Get()->AddObserver(this);
}

AppNotificationHandler::~AppNotificationHandler() {
  ash::MessageCenterAsh::Get()->RemoveObserver(this);
}

void AppNotificationHandler ::OnQuietModeChanged(bool in_quiet_mode) {
  in_quiet_mode_ = in_quiet_mode;
}

void AppNotificationHandler ::SetQuietMode(bool in_quiet_mode) {
  ash::MessageCenterAsh::Get()->SetQuietMode(in_quiet_mode);
}

}  // namespace settings
}  // namespace chromeos

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_DLC_INSTALL_NOTIFICATION_DELEGATE_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_DLC_INSTALL_NOTIFICATION_DELEGATE_H_

#include <vector>

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"
#include "ui/message_center/public/cpp/notification.h"

namespace arc {

class FakeArcDlcInstallNotificationDelegate
    : public ArcDlcInstallNotificationManager::Delegate {
 public:
  FakeArcDlcInstallNotificationDelegate();

  ~FakeArcDlcInstallNotificationDelegate() override;

  void DisplayNotification(
      const message_center::Notification& notification) override;

  const std::vector<message_center::Notification>& displayed_notifications()
      const {
    return displayed_notifications_;
  }

 private:
  std::vector<message_center::Notification> displayed_notifications_;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_DLC_INSTALL_NOTIFICATION_DELEGATE_H_

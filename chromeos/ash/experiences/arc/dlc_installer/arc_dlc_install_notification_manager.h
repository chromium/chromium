// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALL_NOTIFICATION_MANAGER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALL_NOTIFICATION_MANAGER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/account_id/account_id.h"
#include "ui/message_center/public/cpp/notification.h"

namespace arc {

// Represents the type of notification to be displayed.
enum class NotificationType {
  kArcVmPreloadStarted,
  kArcVmPreloadSucceeded,
  kArcVmPreloadFailed
};

// Manages notifications for ARC DLC installation processes.
class ArcDlcInstallNotificationManager {
 public:
  // Interface for handling notification display logic.
  class Delegate {
   public:
    virtual void DisplayNotification(
        const message_center::Notification& notification) = 0;

    virtual ~Delegate() = default;
  };

  ArcDlcInstallNotificationManager(std::unique_ptr<Delegate> delegate,
                                   const AccountId& account_id);
  ~ArcDlcInstallNotificationManager();

  // Displays a notification of the specified type.
  void Show(NotificationType notification_type);

 private:
  // Delegate for managing notification display.
  std::unique_ptr<Delegate> delegate_;

  // Account id associated with notifications.
  const AccountId account_id_;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_DLC_INSTALLER_ARC_DLC_INSTALL_NOTIFICATION_MANAGER_H_

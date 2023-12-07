// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_REBOOT_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_REBOOT_NOTIFICATION_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/device_scheduled_reboot/scheduled_reboot_dialog.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace base {
class Time;
}

namespace ash {

// Id of the pending reboot notification
extern const char kPendingRebootNotificationId[];

// Id of the post reboot notification
extern const char kPostRebootNotificationId[];
}  // namespace ash

// This class is responsible for creating and managing notifications about the
// reboot when DeviceScheduledRebootPolicy is set.
class RebootNotificationController {
 public:
  RebootNotificationController();
  RebootNotificationController(const RebootNotificationController&) = delete;
  RebootNotificationController& operator=(const RebootNotificationController&) =
      delete;
  ~RebootNotificationController();

  // Only show notification if the user is in session and kiosk session is not
  // in progress.
  void MaybeShowPendingRebootNotification(
      const base::Time& reboot_time,
      base::RepeatingClosure reboot_callback);

  // Only show dialog if the user is in session and kiosk session is not
  // in progress.
  void MaybeShowPendingRebootDialog(const base::Time& reboot_time,
                                    base::OnceClosure reboot_callback);

  // Shows notification after the reboot, only if the user is in session and
  // kiosk session is not in progress.
  void MaybeShowPostRebootNotification() const;

  // Close pending reboot notification.
  void CloseRebootNotification() const;

  void CloseRebootDialog();

 protected:
  // Only notify in-session users that are not running in kiosk mode.
  virtual bool ShouldNotifyUser() const;

 private:
  void ShowNotification(
      const std::string& id,
      const std::u16string& title,
      const std::u16string& message,
      const message_center::RichNotificationData& data,
      scoped_refptr<message_center::NotificationDelegate> delegate) const;

  // Button click callback.
  void HandleNotificationClick(std::optional<int> button_index) const;

  // Dialog notifying the user about the pending reboot.
  std::unique_ptr<ScheduledRebootDialog> scheduled_reboot_dialog_;

  // Callback to run on notification button click.
  base::RepeatingClosure notification_callback_;
  base::WeakPtrFactory<RebootNotificationController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DEVICE_SCHEDULED_REBOOT_REBOOT_NOTIFICATION_CONTROLLER_H_

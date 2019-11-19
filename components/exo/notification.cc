// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/notification.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace exo {
namespace {

// Ref-counted delegate for handling events on notification with callbacks.
class NotificationDelegate : public message_center::NotificationDelegate {
 public:
  NotificationDelegate(
      const base::RepeatingCallback<void(bool)>& close_callback,
      const base::RepeatingCallback<void(const base::Optional<int>&)>&
          click_callback)
      : close_callback_(close_callback), click_callback_(click_callback) {}

  // message_center::NotificationDelegate:
  void Close(bool by_user) override {
    if (!close_callback_)
      return;
    close_callback_.Run(by_user);
  }

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (!click_callback_)
      return;
    click_callback_.Run(button_index);
  }

 private:
  // The destructor is private since this class is ref-counted.
  ~NotificationDelegate() override = default;

  const base::RepeatingCallback<void(bool)> close_callback_;
  const base::RepeatingCallback<void(const base::Optional<int>&)>
      click_callback_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDelegate);
};

}  // namespace

Notification::Notification(
    const std::string& title,
    const std::string& message,
    const std::string& display_source,
    const std::string& notification_id,
    const std::string& notifier_id,
    const std::vector<std::string>& buttons,
    const base::RepeatingCallback<void(bool)>& close_callback,
    const base::RepeatingCallback<void(const base::Optional<int>&)>&
        click_callback)
    : notification_id_(notification_id) {
  // Currently, exo::Notification is used only for Crostini notifications.
  // TODO(toshikikikuchi): When this class is used for other reasons,
  // re-consider the way to set the notifier type.
  auto notifier = message_center::NotifierId(
      message_center::NotifierType::CROSTINI_APPLICATION, notifier_id);
  notifier.profile_id = ash::Shell::Get()
                            ->session_controller()
                            ->GetPrimaryUserSession()
                            ->user_info.account_id.GetUserEmail();

  message_center::RichNotificationData data;
  for (const auto& button : buttons)
    data.buttons.emplace_back(base::UTF8ToUTF16(button));

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      base::UTF8ToUTF16(title), base::UTF8ToUTF16(message), gfx::Image(),
      base::UTF8ToUTF16(display_source), GURL(), notifier, data,
      base::MakeRefCounted<NotificationDelegate>(close_callback,
                                                 click_callback));

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void Notification::Close() {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                           false /* by_user */);
}

}  // namespace exo

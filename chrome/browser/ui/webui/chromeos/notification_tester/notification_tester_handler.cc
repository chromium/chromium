// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/notification_tester/notification_tester_handler.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {

NotificationTesterHandler::NotificationTesterHandler() = default;

NotificationTesterHandler::~NotificationTesterHandler() = default;

void NotificationTesterHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "generateNotificationForm",
      base::BindRepeating(
          &NotificationTesterHandler::HandleGenerateNotificationForm,
          base::Unretained(this)));
}

void NotificationTesterHandler::HandleGenerateNotificationForm(
    const base::Value::List& args) {
  AllowJavascript();

  // Parse JS args.
  std::u16string title = base::UTF8ToUTF16(args[0].GetString());
  std::u16string message = base::UTF8ToUTF16(args[1].GetString());

  GenerateNotification(title, message);
}

void NotificationTesterHandler::GenerateNotification(
    const std::u16string& title,
    const std::u16string& message) {
  std::u16string display_source = u"Sample Display Source";
  GURL origin_url("https://test-url.xyz");
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, "test notifier id",
      ash::NotificationCatalogName::kTestCatalogName);
  message_center::RichNotificationData optional_fields;

  // Delegate does nothing.
  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](absl::optional<int> button_index) {}));

  // Generate a unique notification id based on the system time in ms.
  auto current_time_in_ms =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
  std::string notification_id = base::NumberToString(current_time_in_ms);

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
          message, display_source, origin_url, notifier_id, optional_fields,
          delegate, kTerminalSshIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace chromeos

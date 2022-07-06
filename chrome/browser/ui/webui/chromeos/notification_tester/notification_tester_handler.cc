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
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/vector_icon_types.h"
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

  // Unpack JS args.
  const base::Value::Dict* notifObj = args[0].GetIfDict();
  DCHECK(notifObj);

  const std::string* title = notifObj->FindString("title");
  DCHECK(title);

  const std::string* message = notifObj->FindString("message");
  DCHECK(message);

  const std::string* icon = notifObj->FindString("icon");
  DCHECK(icon);

  const std::string* image = notifObj->FindString("richDataImage");
  DCHECK(image);

  // Generate Notification.
  std::u16string display_source = u"Sample Display Source";
  GURL origin_url("https://test-url.xyz");
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, "test notifier id",
      ash::NotificationCatalogName::kTestCatalogName);

  message_center::RichNotificationData optional_fields;
  SetNotificationImage(*image, optional_fields);

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
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          base::UTF8ToUTF16(*title), base::UTF8ToUTF16(*message),
          display_source, origin_url, notifier_id, optional_fields, delegate,
          GetNotificationIcon(*icon),
          message_center::SystemNotificationWarningLevel::NORMAL);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

const gfx::VectorIcon& NotificationTesterHandler::GetNotificationIcon(
    const std::string& icon) {
  if (icon == "kTerminalSshIcon") {
    return kTerminalSshIcon;
  } else if (icon == "kCreditCardIcon") {
    return kCreditCardIcon;
  } else if (icon == "kSmartphoneIcon") {
    return kSmartphoneIcon;
  }

  return gfx::kNoneIcon;  // Default Case
}

// TODO(crbug/1341401): Need to switch from SetNotification...() to
// GetNotification...()
void NotificationTesterHandler::SetNotificationImage(
    const std::string& image,
    message_center::RichNotificationData& optional_fields) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (image == "google_logo_small_png") {
    optional_fields.image = rb.GetImageNamed(IDR_LOGO_GOOGLE_COLOR_90);
  } else if (image == "chromium_logo_large_png") {
    optional_fields.image = rb.GetImageNamed(IDR_CHROME_APP_ICON_192);
  }
}

}  // namespace chromeos

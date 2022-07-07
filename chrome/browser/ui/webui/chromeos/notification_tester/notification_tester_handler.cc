// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/notification_tester/notification_tester_handler.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/models/image_model.h"
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

  message_center::RichNotificationData optional_fields =
      DictToOptionalFields(notifObj);

  auto notification_icon = GetNotificationIconFromString(*icon);

  // Delegate does nothing.
  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](absl::optional<int> button_index) {}));

  // Generate a unique notification id based on the system time in ms.
  auto current_time_in_ms =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
  std::string notification_id = base::NumberToString(current_time_in_ms);

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      notification_id, base::UTF8ToUTF16(*title), base::UTF8ToUTF16(*message),
      notification_icon, display_source, origin_url, notifier_id,
      optional_fields, delegate);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

const ui::ImageModel NotificationTesterHandler::GetNotificationIconFromString(
    const std::string& icon_name) {
  if (icon_name == "chromium_logo") {
    return ui::ImageModel::FromResourceId(IDR_PRODUCT_LOGO_32);
  } else if (icon_name == "google_logo") {
    return ui::ImageModel::FromResourceId(IDR_LOGO_GOOGLE_COLOR_90);
  } else if (icon_name == "warning") {
    return ui::ImageModel::FromResourceId(IDR_RESET_WARNING);
  }

  return ui::ImageModel();
}

const gfx::Image NotificationTesterHandler::GetRichDataImageFromString(
    const std::string& image_name) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (image_name == "google_logo_small_png") {
    return rb.GetImageNamed(IDR_LOGO_GOOGLE_COLOR_90);
  } else if (image_name == "chromium_logo_large_png") {
    return rb.GetImageNamed(IDR_CHROME_APP_ICON_192);
  }

  return gfx::Image();
}

const gfx::VectorIcon&
NotificationTesterHandler::GetRichDataSmallImageFromString(
    const std::string& small_image_name) {
  if (small_image_name == "kTerminalSshIcon") {
    return kTerminalSshIcon;
  } else if (small_image_name == "kCreditCardIcon") {
    return kCreditCardIcon;
  } else if (small_image_name == "kSmartphoneIcon") {
    return kSmartphoneIcon;
  }

  return gfx::kNoneIcon;
}

std::vector<message_center::ButtonInfo>
NotificationTesterHandler::GetRichDataButtons(int num_buttons) {
  std::vector<message_center::ButtonInfo> buttons;
  for (int i = 0; i < num_buttons; i++) {
    buttons.emplace_back(u"Test Btn " + base::NumberToString16(i));
  }
  return buttons;
}

std::vector<message_center::NotificationItem>
NotificationTesterHandler::GetRichDataNotifItems(int num_items) {
  std::vector<message_center::NotificationItem> items;
  for (int i = 0; i < num_items; i++) {
    items.push_back({u"Item " + base::NumberToString16(i), u"item message"});
  }
  return items;
}

message_center::RichNotificationData
NotificationTesterHandler::DictToOptionalFields(
    const base::Value::Dict* notifObj) {
  message_center::RichNotificationData optional_fields;
  // Unpack notification object and populate optional_fields.
  const std::string* image = notifObj->FindString("richDataImage");
  DCHECK(image);
  optional_fields.image = GetRichDataImageFromString(*image);

  const std::string* small_image = notifObj->FindString("richDataSmallImage");
  DCHECK(small_image);
  optional_fields.vector_small_image =
      &GetRichDataSmallImageFromString(*small_image);

  absl::optional<bool> never_timeout =
      notifObj->FindBool("richDataNeverTimeout");
  DCHECK(never_timeout);
  optional_fields.never_timeout = never_timeout.value();

  absl::optional<int> priority = notifObj->FindInt("richDataPriority");
  DCHECK(priority);
  optional_fields.priority = priority.value();

  absl::optional<bool> pinned = notifObj->FindBool("richDataPinned");
  DCHECK(pinned);
  optional_fields.pinned = pinned.value();

  absl::optional<bool> renotify = notifObj->FindBool("richDataRenotify");
  DCHECK(renotify);
  optional_fields.renotify = renotify.value();

  absl::optional<bool> show_snooze = notifObj->FindBool("richDataShowSnooze");
  DCHECK(show_snooze);
  optional_fields.should_show_snooze_button = show_snooze.value();

  absl::optional<bool> show_settings =
      notifObj->FindBool("richDataShowSettings");
  DCHECK(show_settings);
  optional_fields.settings_button_handler =
      show_settings.value() ? message_center::SettingsButtonHandler::INLINE
                            : message_center::SettingsButtonHandler::NONE;

  absl::optional<int> num_buttons = notifObj->FindInt("richDataNumButtons");
  DCHECK(num_buttons);
  optional_fields.buttons = GetRichDataButtons(num_buttons.value());

  // Set additional fields for specific notification types.
  absl::optional<int> notification_type = notifObj->FindInt("notificationType");
  DCHECK(notification_type);

  if (notification_type ==
      message_center::NotificationType::NOTIFICATION_TYPE_PROGRESS) {
    absl::optional<int> progress = notifObj->FindInt("richDataProgress");
    DCHECK(progress);
    optional_fields.progress = progress.value();

    const std::string* progress_status =
        notifObj->FindString("richDataProgressStatus");
    DCHECK(progress_status);
    optional_fields.progress_status = base::UTF8ToUTF16(*progress_status);
  } else if (notification_type ==
             message_center::NotificationType::NOTIFICATION_TYPE_MULTIPLE) {
    absl::optional<int> num_items = notifObj->FindInt("richDataNumNotifItems");
    DCHECK(num_items);
    optional_fields.items = GetRichDataNotifItems(num_items.value());
  }

  return optional_fields;
}

}  // namespace chromeos

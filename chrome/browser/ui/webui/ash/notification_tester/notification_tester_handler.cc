// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/notification_tester/notification_tester_handler.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

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

  // Set notification ID to the system time (always unique) if specified.
  const std::string* id = notifObj->FindString("id");
  DCHECK(id);
  std::string notification_id = *id;
  if (*id == "random") {
    auto current_time_in_ms =
        base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
    notification_id = base::NumberToString(current_time_in_ms);
  }

  const std::string* title = notifObj->FindString("title");
  DCHECK(title);

  const std::string* message = notifObj->FindString("message");
  DCHECK(message);

  const std::string* icon = notifObj->FindString("icon");
  DCHECK(icon);
  auto notification_icon = GetNotificationIconFromString(*icon);

  const std::string* image = notifObj->FindString("richDataImage");
  DCHECK(image);

  const std::string* display_source = notifObj->FindString("displaySource");
  DCHECK(display_source);

  const std::string* origin_url_str = notifObj->FindString("originURL");
  DCHECK(origin_url_str);
  GURL origin_url(*origin_url_str);

  std::optional<int> warning_level_int = notifObj->FindInt("warningLevel");
  DCHECK(warning_level_int);
  auto warning_level =
      static_cast<message_center::SystemNotificationWarningLevel>(
          warning_level_int.value());

  std::optional<int> notification_type_int =
      notifObj->FindInt("notificationType");
  DCHECK(notification_type_int);
  auto notification_type = static_cast<message_center::NotificationType>(
      notification_type_int.value());

  // notifier_id should be constructed differently if the notifier type is
  // message_center::NotifierType::WEB_PAGE.
  std::optional<int> notifier_type = notifObj->FindInt("notifierType");
  DCHECK(notifier_type);
  message_center::NotifierId notifier_id;
  if (notifier_type.value() ==
      static_cast<int>(message_center::NotifierType::WEB_PAGE)) {
    notifier_id = message_center::NotifierId(origin_url);
    // The profile_id must be non-empty to enable notification grouping.
    notifier_id.profile_id = "test-profile-id@gmail.com";
  } else {
    notifier_id = message_center::NotifierId(
        static_cast<message_center::NotifierType>(notifier_type.value()),
        "test notifier id", NotificationCatalogName::kTestCatalogName);
  }

  // Create RichNotificationData object.
  message_center::RichNotificationData optional_fields =
      DictToOptionalFields(notifObj);

  // Delegate does nothing.
  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](std::optional<int> button_index) {}));

  auto notification = std::make_unique<message_center::Notification>(
      notification_type, notification_id, base::UTF8ToUTF16(*title),
      base::UTF8ToUTF16(*message), notification_icon,
      base::UTF8ToUTF16(*display_source), origin_url, notifier_id,
      optional_fields, delegate);

  ui::ColorId color_id = cros_tokens::kCrosSysPrimary;
  if (chromeos::features::IsJellyEnabled()) {
    switch (warning_level) {
      case message_center::SystemNotificationWarningLevel::NORMAL:
        color_id = cros_tokens::kCrosSysPrimary;
        break;
      case message_center::SystemNotificationWarningLevel::WARNING:
        color_id = cros_tokens::kCrosSysWarning;
        break;
      case message_center::SystemNotificationWarningLevel::CRITICAL_WARNING:
        color_id = cros_tokens::kCrosSysError;
        break;
    }
    notification->set_accent_color_id(color_id);
  }

  notification->set_system_notification_warning_level(warning_level);

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
    items.emplace_back(u"Item " + base::NumberToString16(i), u"item message");
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

  std::optional<bool> never_timeout =
      notifObj->FindBool("richDataNeverTimeout");
  DCHECK(never_timeout);
  optional_fields.never_timeout = never_timeout.value();

  std::optional<int> priority = notifObj->FindInt("richDataPriority");
  DCHECK(priority);
  optional_fields.priority = priority.value();

  std::optional<int> num_mins_since_received =
      notifObj->FindInt("richDataTimestamp");
  DCHECK(num_mins_since_received);
  optional_fields.timestamp =
      base::Time::Now() - base::Minutes(num_mins_since_received.value());

  std::optional<bool> pinned = notifObj->FindBool("richDataPinned");
  DCHECK(pinned);
  optional_fields.pinned = pinned.value();

  std::optional<bool> show_snooze = notifObj->FindBool("richDataShowSnooze");
  DCHECK(show_snooze);
  optional_fields.should_show_snooze_button = show_snooze.value();

  std::optional<bool> show_settings =
      notifObj->FindBool("richDataShowSettings");
  DCHECK(show_settings);
  optional_fields.settings_button_handler =
      show_settings.value() ? message_center::SettingsButtonHandler::INLINE
                            : message_center::SettingsButtonHandler::NONE;

  std::optional<int> num_buttons = notifObj->FindInt("richDataNumButtons");
  DCHECK(num_buttons);
  optional_fields.buttons = GetRichDataButtons(num_buttons.value());

  // Set additional fields for specific notification types.
  std::optional<int> notification_type = notifObj->FindInt("notificationType");
  DCHECK(notification_type);

  if (notification_type ==
      message_center::NotificationType::NOTIFICATION_TYPE_PROGRESS) {
    std::optional<int> progress = notifObj->FindInt("richDataProgress");
    DCHECK(progress);
    optional_fields.progress = progress.value();

    const std::string* progress_status =
        notifObj->FindString("richDataProgressStatus");
    DCHECK(progress_status);
    optional_fields.progress_status = base::UTF8ToUTF16(*progress_status);
  } else if (notification_type ==
             message_center::NotificationType::NOTIFICATION_TYPE_MULTIPLE) {
    std::optional<int> num_items = notifObj->FindInt("richDataNumNotifItems");
    DCHECK(num_items);
    optional_fields.items = GetRichDataNotifItems(num_items.value());
  }

  return optional_fields;
}

}  // namespace ash

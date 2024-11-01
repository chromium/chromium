// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/notifications/boca_notification_handler.h"

#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash::boca {
void BocaNotificationHandler::HandleSessionStartedNotification(
    message_center::MessageCenter* message_center) {
  if (!message_center) {
    return;
  }
  if (message_center->FindNotificationById(kNotificationId)) {
    return;
  }
  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  optional_fields.priority = message_center::HIGH_PRIORITY;

  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_BOCA_CONNECTED_TO_CLASS_BUTTON_TEXT));
  auto notification = CreateSystemNotificationPtr(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kNotificationId, l10n_util::GetStringUTF16(IDS_BOCA_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(
          IDS_BOCA_CONNECTED_TO_CLASS_NOTIFICATION_MESSAGE),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kPrivacyIndicatorsNotifierId,
                                 NotificationCatalogName::kPrivacyIndicators),
      optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](std::optional<int> button_index) {
            if (!button_index) {
              return;
            }
            BocaAppClient::Get()->LaunchApp();
          })),
      kSecurityIcon,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);

  message_center->AddNotification(std::move(notification));
}
void BocaNotificationHandler::HandleSessionEndedNotification(
    message_center::MessageCenter* message_center) {
  if (!message_center) {
    return;
  }

  auto* notification_exists =
      message_center->FindNotificationById(kNotificationId);
  if (notification_exists) {
    message_center->RemoveNotification(kNotificationId, /*by_user=*/false);
  }
}
void BocaNotificationHandler::HandleCaptionNotification(
    message_center::MessageCenter* message_center,
    bool is_caption_enabled) {
  if (!message_center) {
    return;
  }
  auto* notification = message_center->FindNotificationById(kNotificationId);
  if (!notification) {
    return;
  }
  is_caption_enabled ? notification->set_message(l10n_util::GetStringUTF16(
                           IDS_BOCA_MICROPHONE_IN_USE_NOTIFICATION_MESSAGE))
                     : notification->set_message(l10n_util::GetStringUTF16(
                           IDS_BOCA_CONNECTED_TO_CLASS_NOTIFICATION_MESSAGE));
}

}  // namespace ash::boca

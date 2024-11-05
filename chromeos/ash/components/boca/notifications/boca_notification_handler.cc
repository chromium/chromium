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

namespace {
std::unique_ptr<message_center::Notification> CreateBaseNotificationForMessage(
    std::string notification_id,
    int message_id) {
  message_center::RichNotificationData optional_fields;
  optional_fields.pinned = true;
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_BOCA_CONNECTED_TO_CLASS_BUTTON_TEXT));
  return CreateSystemNotificationPtr(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      notification_id,
      /*title=*/l10n_util::GetStringUTF16(IDS_BOCA_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(message_id),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT,
          BocaNotificationHandler::kSessionNotificationId,
          NotificationCatalogName::kPrivacyIndicators),
      optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](std::optional<int> button_index) {
            if (!button_index) {
              return;
            }
            BocaAppClient::Get()->LaunchApp();
          })),
      kSecurityIcon, message_center::SystemNotificationWarningLevel::NORMAL);
}
}  // namespace

void BocaNotificationHandler::HandleSessionStartedNotification(
    message_center::MessageCenter* message_center) {
  is_session_started_ = true;
  if (!message_center) {
    return;
  }
  message_center->RemoveNotification(kCaptionNotificationId,
                                     /*by_user=*/false);
  if (message_center->FindNotificationById(kSessionNotificationId)) {
    return;
  }
  message_center->AddNotification(CreateBaseNotificationForMessage(
      kSessionNotificationId,
      IDS_BOCA_CONNECTED_TO_CLASS_NOTIFICATION_MESSAGE));
}

void BocaNotificationHandler::HandleSessionEndedNotification(
    message_center::MessageCenter* message_center) {
  is_session_started_ = false;
  if (!message_center) {
    return;
  }
  message_center->RemoveNotification(kSessionNotificationId,
                                     /*by_user=*/false);
  // Remove session caption notification when session ended.
  if (!is_local_caption_enabled_) {
    message_center->RemoveNotification(kCaptionNotificationId,
                                       /*by_user=*/false);
  }
}

void BocaNotificationHandler::HandleCaptionNotification(
    message_center::MessageCenter* message_center,
    bool is_local_caption_enabled,
    bool is_session_caption_enabled) {
  is_local_caption_enabled_ = is_local_caption_enabled;
  is_session_caption_enabled_ = is_session_caption_enabled;
  if (!message_center) {
    return;
  }
  auto* notification =
      message_center->FindNotificationById(kCaptionNotificationId);

  if (is_local_caption_enabled || is_session_caption_enabled) {
    if (notification) {
      return;
    }
    message_center->RemoveNotification(kSessionNotificationId,
                                       /*by_user=*/false);
    message_center->AddNotification(CreateBaseNotificationForMessage(
        kCaptionNotificationId,
        IDS_BOCA_MICROPHONE_IN_USE_NOTIFICATION_MESSAGE));
  } else {
    message_center->RemoveNotification(kCaptionNotificationId,
                                       /*by_user=*/false);

    if (is_session_started_) {
      message_center->AddNotification(CreateBaseNotificationForMessage(
          kSessionNotificationId,
          IDS_BOCA_CONNECTED_TO_CLASS_NOTIFICATION_MESSAGE));
    }
  }
}

}  // namespace ash::boca

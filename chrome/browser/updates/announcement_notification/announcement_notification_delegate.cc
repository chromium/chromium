// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/first_run/first_run.h"  // nogncheck
#include "chrome/browser/notifications/notification_display_service.h"  // nogncheck
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

AnnouncementNotificationDelegate::AnnouncementNotificationDelegate(
    NotificationDisplayService* display_service)
    : display_service_(display_service) {
  DCHECK(display_service_);
}

AnnouncementNotificationDelegate::~AnnouncementNotificationDelegate() = default;

void AnnouncementNotificationDelegate::ShowNotification() {
  auto rich_notification_data = message_center::RichNotificationData();
  message_center::ButtonInfo button1(
      l10n_util::GetStringUTF16(IDS_TOS_NOTIFICATION_ACK_BUTTON_TEXT));
  message_center::ButtonInfo button2(
      l10n_util::GetStringUTF16(IDS_TOS_NOTIFICATION_REVIEW_BUTTON_TEXT));
  rich_notification_data.buttons.push_back(button1);
  rich_notification_data.buttons.push_back(button2);

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kAnnouncementNotificationId,
      l10n_util::GetStringUTF16(IDS_TOS_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_TOS_NOTIFICATION_BODY_TEXT),
      ui::ImageModel(), std::u16string(), GURL(),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT,
          kAnnouncementNotificationId,
          ash::NotificationCatalogName::kAnnouncementNotification),
#else
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kAnnouncementNotificationId),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      rich_notification_data, nullptr /*delegate*/);

  display_service_->Display(NotificationHandler::Type::ANNOUNCEMENT,
                            notification, nullptr /*metadata*/);
}

bool AnnouncementNotificationDelegate::IsFirstRun() {
  return first_run::IsChromeFirstRun();
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/notification_display_client.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace ash::download_status {

namespace {

// Constants -------------------------------------------------------------------

constexpr char kNotificationNotifierId[] =
    "chrome://downloads/notification/id-notifier";

constexpr char kNotificationOrigin[] = "chrome://downloads";

// Helpers ---------------------------------------------------------------------

// NOTE: This function returns a non-empty string indicating the notification
// text, but does not guarantee the presence of a notification.
std::string GetNotificationIdFromGuid(const std::string& guid) {
  return base::StrCat({kNotificationNotifierId, "/", guid});
}

}  // namespace

NotificationDisplayClient::NotificationDisplayClient(Profile* profile)
    : DisplayClient(profile) {
  CHECK(features::IsSysUiDownloadsIntegrationV2Enabled());
}

NotificationDisplayClient::~NotificationDisplayClient() = default;

void NotificationDisplayClient::AddOrUpdate(
    const std::string& guid,
    const DisplayMetadata& display_metadata) {
  // TODO(http://b/310691284): Avoid showing a notification if it has been
  // closed.

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.should_make_spoken_feedback_for_popup_updates = false;
  rich_notification_data.vector_small_image = &kNotificationDownloadIcon;

  // TODO(http://b/310691284): Initialize `notification` with
  // `display_metadata`.
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_PROGRESS,
      GetNotificationIdFromGuid(guid),
      /*title=*/std::u16string(),
      /*message=*/std::u16string(),
      /*icon=*/ui::ImageModel(),
      /*display_source=*/
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_NOTIFICATION_DISPLAY_SOURCE),
      GURL(kNotificationOrigin),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT,
          kNotificationNotifierId,
          NotificationCatalogName::kDownloadNotification),
      rich_notification_data,
      base::MakeRefCounted<message_center::NotificationDelegate>());
  notification.set_fullscreen_visibility(
      message_center::FullscreenVisibility::OVER_USER);

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);
}

void NotificationDisplayClient::Remove(const std::string& guid) {
  NotificationDisplayService::GetForProfile(profile())->Close(
      NotificationHandler::Type::TRANSIENT, GetNotificationIdFromGuid(guid));
}

}  // namespace ash::download_status

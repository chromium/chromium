// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/message_center/metadata_utils.h"

#include "base/strings/utf_string_conversions.h"

using arc::mojom::ArcNotificationData;

namespace ash {

std::unique_ptr<message_center::Notification>
CreateNotificationFromArcNotificationData(
    const message_center::NotificationType notification_type,
    const std::string& notification_id,
    ArcNotificationData* data,
    const message_center::NotifierId notifier_id,
    message_center::RichNotificationData rich_data,
    scoped_refptr<message_center::NotificationDelegate> delegate) {
  const bool is_parent = data->children_data.has_value();

  if (!is_parent) {
    rich_data.progress = std::clamp(
        static_cast<int>(std::round(static_cast<float>(data->progress_current) /
                                    data->progress_max * 100)),
        -1, 100);
  }

  auto notification = std::make_unique<message_center::Notification>(
      notification_type, notification_id, base::UTF8ToUTF16(data->title),
      base::UTF8ToUTF16(data->message), ui::ImageModel(),
      /*display_source=*/
      base::UTF8ToUTF16(data->app_display_name.value_or(std::string())),
      /*origin_url=*/GURL(), notifier_id, rich_data, delegate);

  if (is_parent) {
    notification->SetGroupParent();
  }

  return notification;
}

}  // namespace ash

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/camera/upload_done_notification.h"

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chromeos/ash/experiences/camera/camera_notification_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace {

constexpr char kUploadDoneNotificationId[] =
    "skyvault_camera_upload_done_notification";

void OnButtonPressed(base::RepeatingClosure edit_callback,
                     base::RepeatingClosure delete_callback,
                     std::optional<int> button_index) {
  if (!button_index) {
    return;
  }
  if (*button_index == 0) {
    edit_callback.Run();
  } else if (*button_index == 1) {
    delete_callback.Run();
  }

  message_center::MessageCenter::Get()->RemoveNotification(
      kUploadDoneNotificationId, /*by_user=*/true);
}

}  // namespace

void CreateUploadDoneNotification(bool onedrive,
                                  const gfx::Image& thumbnail,
                                  const base::FilePath& file_path,
                                  base::RepeatingClosure edit_callback,
                                  base::RepeatingClosure delete_callback) {
  message_center::RichNotificationData options;
  std::vector<message_center::ButtonInfo> buttons_info;
  buttons_info.emplace_back(l10n_util::GetStringUTF16(
      IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_DONE_EDIT_BUTTON));
  buttons_info.emplace_back(l10n_util::GetStringUTF16(
      IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_DONE_DELETE_BUTTON));
  options.buttons = buttons_info;
  options.image = thumbnail;
  options.image_path = file_path;
  options.vector_small_image = &chromeos::kCameraIcon;

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, kUploadDoneNotificationId,
      /*title=*/
      l10n_util::GetStringUTF16(
          GetCameraUploadDoneTitleId(onedrive, file_path)),
      /*message=*/
      l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_DONE_MESSAGE),
      ui::ImageModel(),
      l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_DISPLAY_SOURCE),
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kUploadDoneNotificationId,
                                 ash::NotificationCatalogName::kCameraUpload),
      options,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&OnButtonPressed, std::move(edit_callback),
                              std::move(delete_callback))));
  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);
  message_center->RemoveNotification(kUploadDoneNotificationId,
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

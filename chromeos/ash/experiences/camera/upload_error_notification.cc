// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/camera/upload_error_notification.h"

#include <string>

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

constexpr char kUploadErrorNotificationId[] =
    "skyvault_camera_upload_error_notification";

void OnButtonPressed(base::RepeatingClosure retake_callback,
                     std::optional<int> button_index) {
  if (!button_index) {
    return;
  }
  if (*button_index == 0) {
    retake_callback.Run();
  }

  message_center::MessageCenter::Get()->RemoveNotification(
      kUploadErrorNotificationId, /*by_user=*/false);
}

}  // namespace

void CreateUploadErrorNotification(const gfx::Image& thumbnail,
                                   const base::FilePath& file_path,
                                   base::RepeatingClosure retake_callback) {
  const auto upload_error_strings =
      GetCameraUploadErrorStringsFromFilename(file_path);

  message_center::RichNotificationData options;
  std::vector<message_center::ButtonInfo> buttons_info;
  buttons_info.emplace_back(
      l10n_util::GetStringUTF16(upload_error_strings.retake_button));
  buttons_info.emplace_back(l10n_util::GetStringUTF16(
      IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_ERROR_DISMISS_BUTTON));
  options.buttons = buttons_info;
  options.image = thumbnail;
  options.image_path = file_path;
  options.vector_small_image = &chromeos::kCameraIcon;

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, kUploadErrorNotificationId,
      /*title=*/l10n_util::GetStringUTF16(upload_error_strings.title),
      /*message=*/l10n_util::GetStringUTF16(upload_error_strings.message),
      ui::ImageModel(),
      l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_DISPLAY_SOURCE),
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kUploadErrorNotificationId,
                                 ash::NotificationCatalogName::kCameraUpload),
      options,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&OnButtonPressed, std::move(retake_callback))));
  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);
  message_center->RemoveNotification(kUploadErrorNotificationId,
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

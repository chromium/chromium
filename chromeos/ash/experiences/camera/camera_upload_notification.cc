// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/camera/camera_upload_notification.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/experiences/camera/camera_save_handler.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace {

constexpr char kUploadNotificationId[] = "skyvault_camera_upload_notification";

}  // namespace

CameraUploadNotification::CameraUploadNotification(
    CameraSaveHandler::FileSaveDestination destination,
    base::OnceClosure cancel_closure)
    : destination_(destination), cancel_closure_(std::move(cancel_closure)) {
  // Set up indefinite progress first.
  UpdateProgress(-1, 1);
}

CameraUploadNotification::~CameraUploadNotification() {
  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);
  message_center->RemoveNotification(kUploadNotificationId,
                                     /*by_user=*/false);
}

void CameraUploadNotification::UpdateProgress(double percent,
                                              int number_of_uploads) {
  message_center::RichNotificationData options;
  options.vector_small_image = &chromeos::kCameraIcon;
  options.buttons = {message_center::ButtonInfo(l10n_util::GetStringUTF16(
      IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_CANCEL_BUTTON))};

  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, kUploadNotificationId,
      /*title=*/std::u16string(),
      /*message=*/std::u16string(), ui::ImageModel(),
      l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_DISPLAY_SOURCE),
      GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kUploadNotificationId,
                                 ash::NotificationCatalogName::kCameraUpload),
      options,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&CameraUploadNotification::OnButtonPressed,
                              weak_ptr_factory_.GetWeakPtr())));
  notification->set_progress(static_cast<int>(percent));
  std::u16string title;
  std::u16string status;
  CHECK_GE(number_of_uploads, 0);
  if (number_of_uploads == 1) {
    title = l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_TITLE);
    status = l10n_util::GetStringUTF16(
        destination_ == CameraSaveHandler::FileSaveDestination::kOneDrive
            ? IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_ONEDRIVE_MESSAGE
            : IDS_POLICY_SKYVAULT_CAMERA_UPLOAD_GOOGLE_DRIVE_MESSAGE);
  } else {
    title = l10n_util::GetStringUTF16(
        IDS_POLICY_SKYVAULT_CAMERA_MULTI_UPLOAD_TITLE);
    status = l10n_util::GetStringFUTF16Int(
        destination_ == CameraSaveHandler::FileSaveDestination::kOneDrive
            ? IDS_POLICY_SKYVAULT_CAMERA_MULTI_UPLOAD_ONEDRIVE_MESSAGE
            : IDS_POLICY_SKYVAULT_CAMERA_MULTI_UPLOAD_GOOGLE_DRIVE_MESSAGE,
        number_of_uploads);
  }
  notification->set_title(title);
  notification->set_progress_status(status);
  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);
  if (message_center->FindNotificationById(kUploadNotificationId)) {
    message_center->UpdateNotification(kUploadNotificationId,
                                       std::move(notification));
  } else {
    message_center->AddNotification(std::move(notification));
  }
}

void CameraUploadNotification::OnButtonPressed(
    std::optional<int> button_index) {
  if (!button_index || button_index.value() != 0) {
    return;
  }
  if (cancel_closure_) {
    std::move(cancel_closure_).Run();
  }
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_UPLOAD_NOTIFICATION_H_
#define CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_UPLOAD_NOTIFICATION_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/experiences/camera/camera_save_handler.h"
#include "ui/message_center/public/cpp/notification.h"

// This class owns and manages a `message_center::Notification` to display the
// progress of an uploaded camera item.
class CameraUploadNotification {
 public:
  explicit CameraUploadNotification(
      CameraSaveHandler::FileSaveDestination destination,
      base::OnceClosure cancel_closure);
  CameraUploadNotification(const CameraUploadNotification&) = delete;
  CameraUploadNotification& operator=(const CameraUploadNotification&) = delete;
  ~CameraUploadNotification();

  void UpdateProgress(double percent, int number_of_uploads);

 private:
  // Called when "Cancel" button is pressed.
  void OnButtonPressed(std::optional<int> button_index);

  CameraSaveHandler::FileSaveDestination destination_;

  // Called when upload should be cancelled.
  base::OnceClosure cancel_closure_;

  base::WeakPtrFactory<CameraUploadNotification> weak_ptr_factory_{this};
};

#endif  // CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_UPLOAD_NOTIFICATION_H_

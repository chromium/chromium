// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_NOTIFICATION_UTIL_H_
#define CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_NOTIFICATION_UTIL_H_

namespace base {
class FilePath;
}

struct SignInNotificationIds {
  int title;
  int message;
};

struct UploadErrorIds {
  int title;
  int message;
  int retake_button;
};

// Takes the file name and returns title and message string IDs for camera
// OneDrive sign-in notification. File name is expected to have a supported
// extension.
SignInNotificationIds GetCameraSignInStringsFromFilename(
    const base::FilePath& file);

// Returns the title string ID for camera upload done notification based on
// the type of `file` and whether it is uploaded to `onedrive` or Google Drive.
int GetCameraUploadDoneTitleId(bool onedrive, const base::FilePath& file);

// Returns the upload error notification string IDs based on the type of `file`.
UploadErrorIds GetCameraUploadErrorStringsFromFilename(
    const base::FilePath& file);

#endif  // CHROMEOS_ASH_EXPERIENCES_CAMERA_CAMERA_NOTIFICATION_UTIL_H_

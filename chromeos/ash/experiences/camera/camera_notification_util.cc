// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/camera/camera_notification_util.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr auto kCameraSigninStringsMap =
    base::MakeFixedFlatMap<std::string_view, SignInNotificationIds>(
        {{"image/jpeg",
          {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_PHOTO,
           IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_PHOTO}},
         {"image/gif",
          {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_VIDEO,
           IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_VIDEO}},
         {"video/mp4",
          {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_VIDEO,
           IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_VIDEO}},
         {"application/pdf",
          {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_SCAN,
           IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_SCAN}}});

struct UploadDoneNotificationTitleIds {
  int onedrive_id;
  int google_drive_id;
};

constexpr auto kCameraUploadDoneTitleMap =
    base::MakeFixedFlatMap<std::string_view, UploadDoneNotificationTitleIds>(
        {{"image/jpeg",
          {IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_DONE_TITLE_ONEDRIVE,
           IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_DONE_TITLE_GOOGLE_DRIVE}},
         {"image/gif",
          {IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_DONE_TITLE_ONEDRIVE,
           IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_DONE_TITLE_GOOGLE_DRIVE}},
         {"video/mp4",
          {IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_DONE_TITLE_ONEDRIVE,
           IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_DONE_TITLE_GOOGLE_DRIVE}},
         {"application/pdf",
          {IDS_POLICY_SKYVAULT_CAMERA_SCAN_UPLOAD_DONE_TITLE_ONEDRIVE,
           IDS_POLICY_SKYVAULT_CAMERA_SCAN_UPLOAD_DONE_TITLE_GOOGLE_DRIVE}}});

constexpr auto kCameraUploadErrorMap =
    base::MakeFixedFlatMap<std::string_view, UploadErrorIds>(
        {{"image/jpeg",
          {IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_ERROR_TITLE,
           IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_ERROR_MESSAGE,
           IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_ERROR_RETAKE_BUTTON}},
         {"image/gif",
          {IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_ERROR_TITLE,
           IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_ERROR_MESSAGE,
           IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_ERROR_RETAKE_BUTTON}},
         {"video/mp4",
          {IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_ERROR_TITLE,
           IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_ERROR_MESSAGE,
           IDS_POLICY_SKYVAULT_CAMERA_VIDEO_UPLOAD_ERROR_RETAKE_BUTTON}},
         {"application/pdf",
          {IDS_POLICY_SKYVAULT_CAMERA_SCAN_UPLOAD_ERROR_TITLE,
           IDS_POLICY_SKYVAULT_CAMERA_SCAN_UPLOAD_ERROR_MESSAGE,
           IDS_POLICY_SKYVAULT_CAMERA_SCAN_UPLOAD_ERROR_RETAKE_BUTTON}}});
}  // namespace

SignInNotificationIds GetCameraSignInStringsFromFilename(
    const base::FilePath& file) {
  std::string mime_type;
  if (net::GetWellKnownMimeTypeFromFile(file, &mime_type)) {
    if (const auto it = kCameraSigninStringsMap.find(mime_type);
        it != kCameraSigninStringsMap.end()) {
      return it->second;
    }
  }
  DLOG(FATAL) << "Unsupported extension: " << file.Extension();
  // Use "photo" as fallback.
  return {IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_TITLE_PHOTO,
          IDS_POLICY_SKYVAULT_CAMERA_SIGN_IN_MESSAGE_PHOTO};
}

int GetCameraUploadDoneTitleId(bool onedrive, const base::FilePath& file) {
  std::string mime_type;
  if (net::GetWellKnownMimeTypeFromFile(file, &mime_type)) {
    if (const auto it = kCameraUploadDoneTitleMap.find(mime_type);
        it != kCameraUploadDoneTitleMap.end()) {
      return onedrive ? it->second.onedrive_id : it->second.google_drive_id;
    }
  }
  DLOG(FATAL) << "Unsupported extension: " << file.Extension();
  // Use "photo" as fallback.
  return onedrive
             ? IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_DONE_TITLE_ONEDRIVE
             : IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_DONE_TITLE_GOOGLE_DRIVE;
}

UploadErrorIds GetCameraUploadErrorStringsFromFilename(
    const base::FilePath& file) {
  std::string mime_type;
  if (net::GetWellKnownMimeTypeFromFile(file, &mime_type)) {
    if (const auto it = kCameraUploadErrorMap.find(mime_type);
        it != kCameraUploadErrorMap.end()) {
      return it->second;
    }
  }
  DLOG(FATAL) << "Unsupported extension: " << file.Extension();
  // Use "photo" as fallback.
  return {IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_ERROR_TITLE,
          IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_ERROR_MESSAGE,
          IDS_POLICY_SKYVAULT_CAMERA_PHOTO_UPLOAD_ERROR_RETAKE_BUTTON};
}

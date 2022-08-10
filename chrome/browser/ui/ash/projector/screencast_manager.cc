// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include <memory>
#include <vector>

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "ash/webui/projector_app/projector_screencast.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ui/ash/projector/projector_drivefs_provider.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"

namespace ash {

namespace {

void OnVideoFilePathLocated(
    std::unique_ptr<ProjectorScreencastVideo> video,
    ProjectorAppClient::OnGetVideoCallback callback,
    absl::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths) {
  const std::string& video_id = video->file_id;
  if (!paths || paths.value().size() != 1u) {
    std::move(callback).Run(
        /*video=*/nullptr,
        base::StringPrintf("Failed to find DriveFS path with video file id=%s",
                           video_id.c_str()));
    return;
  }

  const auto& path_or_error = paths.value()[0];
  if (path_or_error->is_error() || !path_or_error->is_path()) {
    std::move(callback).Run(
        /*video=*/nullptr,
        base::StringPrintf("Failed to fetch DriveFS file with video file id=%s "
                           "and error code=%d",
                           video_id.c_str(), path_or_error->get_error()));
    return;
  }

  const base::FilePath& relative_drivefs_path = path_or_error->get_path();
  if (!relative_drivefs_path.MatchesExtension(kProjectorMediaFileExtension)) {
    std::move(callback).Run(
        /*video=*/nullptr,
        base::StringPrintf("Failed to fetch video file with video file id=%s",
                           video_id.c_str()));
    return;
  }

  const base::FilePath& mounted_path =
      ProjectorDriveFsProvider::GetDriveFsMountPointPath();
  const base::FilePath& video_path = mounted_path.Append(relative_drivefs_path);
  // TODO(b/205334821): Find the video duration and validate the media file.
  LaunchProjectorAppWithFiles({video_path});

  std::move(callback).Run(std::move(video), /*error_message=*/std::string());
}

}  // namespace

ScreencastManager::ScreencastManager() = default;
ScreencastManager::~ScreencastManager() = default;

void ScreencastManager::GetVideo(
    const std::string& video_file_id,
    const std::string& resource_key,
    ProjectorAppClient::OnGetVideoCallback callback) const {
  auto video = std::make_unique<ProjectorScreencastVideo>();
  video->file_id = video_file_id;
  // TODO(b/237089852): Handle the resource key once LocateFilesByItemIds()
  // supports it.

  drive::DriveIntegrationService* integration_service =
      ProjectorDriveFsProvider::GetActiveDriveIntegrationService();
  integration_service->LocateFilesByItemIds(
      {video_file_id}, base::BindOnce(&OnVideoFilePathLocated, std::move(video),
                                      std::move(callback)));
}

}  // namespace ash

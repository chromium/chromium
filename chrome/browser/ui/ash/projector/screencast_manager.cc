// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include <memory>
#include <vector>

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/extensions/file_manager/scoped_suppress_drive_notifications_for_path.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/projector/projector_drivefs_provider.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chrome/services/media_gallery_util/public/cpp/local_media_data_source_factory.h"
#include "chrome/services/media_gallery_util/public/cpp/safe_media_metadata_parser.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

constexpr int kOneSecondInMillisecond = 1000;

void OnMediaMetadataParsed(
    projector::mojom::VideoInfoPtr video,
    ProjectorAppClient::OnGetVideoCallback callback,
    const base::FilePath& video_path,
    std::unique_ptr<SafeMediaMetadataParser> parser_keep_alive,
    bool parse_success,
    chrome::mojom::MediaMetadataPtr metadata,
    std::unique_ptr<std::vector<metadata::AttachedImage>> attached_images) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!parse_success || !metadata || !metadata->duration) {
    // TODO(b/242748137): Add test to cover this error message.
    std::move(callback).Run(
        projector::mojom::GetVideoResult::NewErrorMessage(base::StringPrintf(
            "Failed get media metadata or duration with video file id=%s",
            video->file_id.c_str())));
    return;
  }

  if (metadata->duration < 0) {
    // The video file might be malformed if duration is -1.
    std::move(callback).Run(projector::mojom::GetVideoResult::NewErrorMessage(
        base::StringPrintf("Media might be malformed with video file id=%s",
                           video->file_id.c_str())));
    return;
  }

  video->duration_millis = metadata->duration * kOneSecondInMillisecond;
  // Launches app on UI thread when duration is valid.

  // Even though the video file id is not a file path, we need to pass it to the
  // launch event to match up with the original request.
  base::FilePath video_id_as_path(video->file_id);
  SendFilesToProjectorApp({video_id_as_path, video_path});
  std::move(callback).Run(
      projector::mojom::GetVideoResult::NewVideo(std::move(video)));
}

// Caller of this method requires a sequenced context. Gets video metadata for
// `video_path`, triggers the flow to set `duration_millis` in the given video
// object and triggers the callback. Should not be called on UI thread.
void GetVideoMetadata(const base::FilePath& video_path,
                      projector::mojom::VideoInfoPtr video,
                      ProjectorAppClient::OnGetVideoCallback callback) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  int64_t size_in_byte;
  if (!base::PathExists(video_path) ||
      !base::GetFileSize(video_path, &size_in_byte)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            projector::mojom::GetVideoResult::NewErrorMessage(
                base::StringPrintf(
                    "Path does not exist or cannot read video size with "
                    "video file id=%s",
                    video->file_id.c_str()))));
    return;
  }

  auto parser = std::make_unique<SafeMediaMetadataParser>(
      size_in_byte, kProjectorMediaMimeType,
      /*get_attached_images=*/false,
      std::make_unique<LocalMediaDataSourceFactory>(
          video_path, base::SingleThreadTaskRunner::GetCurrentDefault()));

  // Uses raw pointer since the `parser` is moved before calling Start().
  SafeMediaMetadataParser* parser_ptr = parser.get();
  parser_ptr->Start(base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&OnMediaMetadataParsed, std::move(video),
                     std::move(callback), video_path, std::move(parser))));
}

}  // namespace

// CreateSingleThreadTaskRunner for `video_metadata_task_runner` since
// LocalMediaDataSource requires a single thread context.
ScreencastManager::ScreencastManager()
    : video_metadata_task_runner_(
          base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()})) {}
ScreencastManager::~ScreencastManager() = default;

void ScreencastManager::GetVideo(
    const std::string& video_file_id,
    const std::optional<std::string>& resource_key,
    ProjectorAppClient::OnGetVideoCallback callback) const {
  // TODO(b/237089852): Handle the resource key once LocateFilesByItemIds()
  // supports it.

  drive::DriveIntegrationService* integration_service =
      ProjectorDriveFsProvider::GetActiveDriveIntegrationService();
  integration_service->LocateFilesByItemIds(
      {video_file_id},
      base::BindOnce(&ScreencastManager::OnVideoFilePathLocated,
                     weak_ptr_factory_.GetMutableWeakPtr(), video_file_id,
                     std::move(callback)));
}

void ScreencastManager::ResetScopeSuppressDriveNotifications() {
  suppress_drive_notifications_for_path_.reset();
}

void ScreencastManager::OnVideoFilePathLocated(
    const std::string& video_id,
    ProjectorAppClient::OnGetVideoCallback callback,
    std::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths) {
  if (!paths || paths.value().size() != 1u) {
    std::move(callback).Run(projector::mojom::GetVideoResult::NewErrorMessage(
        base::StringPrintf("Failed to find DriveFS path with video file id=%s",
                           video_id.c_str())));

    return;
  }

  const auto& path_or_error = paths.value()[0];
  if (path_or_error->is_error() || !path_or_error->is_path()) {
    std::move(callback).Run(projector::mojom::GetVideoResult::NewErrorMessage(
        base::StringPrintf("Failed to fetch DriveFS file with video file id=%s "
                           "and error code=%d",
                           video_id.c_str(), path_or_error->get_error())));
    return;
  }

  const base::FilePath& relative_drivefs_path = path_or_error->get_path();
  if (!relative_drivefs_path.MatchesExtension(kProjectorMediaFileExtension)) {
    std::move(callback).Run(projector::mojom::GetVideoResult::NewErrorMessage(
        base::StringPrintf("Failed to fetch video file with video file id=%s",
                           video_id.c_str())));
    return;
  }

  const base::FilePath& mounted_path =
      ProjectorDriveFsProvider::GetDriveFsMountPointPath();
  const base::FilePath& video_path = mounted_path.Append(relative_drivefs_path);

  // Suppresses the notification before calling GetVideoMetadata, which might
  // trigger file syncing event that triggers Drive notifications.
  suppress_drive_notifications_for_path_ =
      std::make_unique<file_manager::ScopedSuppressDriveNotificationsForPath>(
          ProfileManager::GetActiveUserProfile(),
          base::FilePath("/").Append(relative_drivefs_path));

  // Post task to:
  // 1. Fill the video duration which also serves the purpose of video file
  // validation.
  // 2. If the duration is valid, trigger LaunchProjectorAppWithFiles().
  // 3. Trigger `callback`.
  auto video = projector::mojom::VideoInfo::New();
  video->file_id = video_id;
  video_metadata_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GetVideoMetadata, video_path, std::move(video),
                                std::move(callback)));
}

}  // namespace ash

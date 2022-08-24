// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_handler.h"

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "url/gurl.h"

namespace chromeos::cloud_upload {
namespace {

// The default folder where the file should be uploaded.
const char kDestinationFolder[] = "from Chromebook";

storage::FileSystemURL FilePathToFileSystemURL(
    Profile* profile,
    storage::FileSystemContext* file_system_context,
    base::FilePath file_path) {
  GURL url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::util::GetFileManagerURL(), &url)) {
    LOG(ERROR) << "Unable to ConvertAbsoluteFilePathToFileSystemUrl";
    return storage::FileSystemURL();
  }

  return file_system_context->CrackURLInFirstPartyContext(url);
}

}  // namespace

base::FilePath GenerateDestinationPath(Profile* profile) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  return integration_service
             ? integration_service->GetMountPointPath().Append("root").Append(
                   kDestinationFolder)
             : base::FilePath();
}

void UploadToCloud(Profile* profile, const storage::FileSystemURL& file_url) {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);
  // TODO (b/243095484) Define error behavior.
  if (!volume_manager || !volume_manager->io_task_controller()) {
    LOG(ERROR) << "No volume_manager or task_controller";
    return;
  }

  // Filesystem context.
  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileSystemContextForSourceURL(
          profile, file_manager::util::GetFileManagerURL());

  // Source and destination urls.
  std::vector<storage::FileSystemURL> source_urls{file_url};
  base::FilePath destination_folder_path = GenerateDestinationPath(profile);
  // TODO (b/243095484) Define error behavior.
  if (destination_folder_path.empty()) {
    LOG(ERROR) << "Unable to generate destination folder, the drive "
                  "integration service might not be available.";
    return;
  }
  storage::FileSystemURL destination_folder_url = FilePathToFileSystemURL(
      profile, file_system_context, destination_folder_path);

  // TODO (b/242685159) Change copy to move.
  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          file_manager::io_task::OperationType::kCopy, std::move(source_urls),
          std::move(destination_folder_url), profile, file_system_context);

  const auto taskId =
      volume_manager->io_task_controller()->Add(std::move(task));
  LOG(ERROR) << taskId;
}

}  // namespace chromeos::cloud_upload

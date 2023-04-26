// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "content/public/browser/browser_thread.h"

namespace ash::cloud_upload {

storage::FileSystemURL FilePathToFileSystemURL(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::FilePath file_path) {
  GURL url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::util::GetFileManagerURL(), &url)) {
    LOG(ERROR) << "Unable to ConvertAbsoluteFilePathToFileSystemUrl";
    return storage::FileSystemURL();
  }

  return file_system_context->CrackURLInFirstPartyContext(url);
}

file_manager::io_task::OperationType GetOperationTypeForUpload(
    Profile* profile,
    const storage::FileSystemURL& source_url) {
  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile);
  base::WeakPtr<file_manager::Volume> source_volume =
      volume_manager->FindVolumeFromPath(source_url.path());
  DCHECK(source_volume)
      << "Unable to find source volume (source path filesystem_id: "
      << source_url.filesystem_id() << ")";
  return source_volume && source_volume->is_read_only()
             ? file_manager::io_task::OperationType::kCopy
             : file_manager::io_task::OperationType::kMove;
}

}  // namespace ash::cloud_upload

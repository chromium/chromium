// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
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

SourceType GetSourceType(Profile* profile,
                         const storage::FileSystemURL& source_url) {
  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile);
  base::WeakPtr<file_manager::Volume> source_volume =
      volume_manager->FindVolumeFromPath(source_url.path());
  DCHECK(source_volume)
      << "Unable to find source volume (source path filesystem_id: "
      << source_url.filesystem_id() << ")";
  // Local by default.
  if (!source_volume) {
    return SourceType::LOCAL;
  }
  // First, look at whether the filesystem is read-only.
  if (source_volume->is_read_only()) {
    return SourceType::READ_ONLY;
  }
  // Some volume types are generally associated with cloud filesystems.
  if (source_volume->type() == file_manager::VOLUME_TYPE_GOOGLE_DRIVE ||
      source_volume->type() == file_manager::VOLUME_TYPE_SMB ||
      source_volume->type() == file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER) {
    return SourceType::CLOUD;
  }
  // For provided file systems, check whether file system's source data is
  // retrieved over the network.
  if (source_volume->type() == file_manager::VOLUME_TYPE_PROVIDED) {
    const base::FilePath source_path = source_url.path();
    file_system_provider::Service* service =
        file_system_provider::Service::Get(profile);
    std::vector<file_system_provider::ProvidedFileSystemInfo> file_systems =
        service->GetProvidedFileSystemInfoList();
    for (const auto& file_system : file_systems) {
      if (file_system.mount_path().IsParent(source_path)) {
        return file_system.source() ==
                       extensions::FileSystemProviderSource::SOURCE_NETWORK
                   ? SourceType::CLOUD
                   : SourceType::LOCAL;
      }
    }
    // Local if unable to find the provided file system.
    return SourceType::LOCAL;
  }
  // Local by default.
  return SourceType::LOCAL;
}

file_manager::io_task::OperationType GetOperationTypeForUpload(
    Profile* profile,
    const storage::FileSystemURL& source_url) {
  SourceType source_type = GetSourceType(profile, source_url);
  return source_type == SourceType::LOCAL
             ? file_manager::io_task::OperationType::kMove
             : file_manager::io_task::OperationType::kCopy;
}

}  // namespace ash::cloud_upload

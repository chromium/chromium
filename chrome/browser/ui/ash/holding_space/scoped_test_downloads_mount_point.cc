// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/scoped_test_downloads_mount_point.h"

#include "base/files/file_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"

namespace ash {
namespace holding_space {
ScopedTestDownloadsMountPoint::ScopedTestDownloadsMountPoint(Profile* profile)
    : name_(file_manager::util::GetDownloadsMountPointName(profile)) {
  if (!temp_dir_.CreateUniqueTempDir())
    return;

  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      name_, storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(), temp_dir_.GetPath());
  file_manager::util::GetFileSystemContextForExtensionId(
      profile, file_manager::kFileManagerAppId)
      ->external_backend()
      ->GrantFileAccessToExtension(file_manager::kFileManagerAppId,
                                   base::FilePath(name_));
}

ScopedTestDownloadsMountPoint::~ScopedTestDownloadsMountPoint() {
  storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(name_);
}

base::FilePath ScopedTestDownloadsMountPoint::CreateFile(
    const base::FilePath& relative_path,
    const std::string& content) {
  const base::FilePath path = GetRootPath().Append(relative_path);
  if (!base::CreateDirectory(path.DirName()))
    return base::FilePath();
  if (!base::WriteFile(path, content))
    return base::FilePath();
  return path;
}

base::FilePath ScopedTestDownloadsMountPoint::CreateArbitraryFile() {
  return CreateFile(base::FilePath(base::UnguessableToken::Create().ToString()),
                    /*content=*/std::string());
}

}  // namespace holding_space
}  // namespace ash
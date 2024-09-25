// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"

#include "base/files/file_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"

namespace ash {
namespace holding_space {

// static
std::unique_ptr<ScopedTestMountPoint>
ScopedTestMountPoint::CreateAndMountDownloads(Profile* profile) {
  auto mount_point = std::make_unique<ScopedTestMountPoint>(
      file_manager::util::GetDownloadsMountPointName(profile),
      storage::kFileSystemTypeLocal,
      file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY);
  mount_point->Mount(profile);
  return mount_point;
}

ScopedTestMountPoint::ScopedTestMountPoint(
    const std::string& name,
    storage::FileSystemType file_system_type,
    file_manager::VolumeType volume_type)
    : name_(name),
      file_system_type_(file_system_type),
      volume_type_(volume_type) {
  CHECK(temp_dir_.CreateUniqueTempDir());
}

ScopedTestMountPoint::~ScopedTestMountPoint() {
  if (mounted_) {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(name_);
    if (file_manager::VolumeManager::Get(profile_)) {
      file_manager::VolumeManager::Get(profile_)
          ->RemoveVolumeForTesting(  // IN-TEST
              temp_dir_.GetPath(), volume_type_, DeviceType::kUnknown,
              /*read_only=*/false);
    }
  }
}
void ScopedTestMountPoint::Mount(Profile* profile) {
  DCHECK(!profile_);
  DCHECK(!mounted_);
  profile_ = profile;
  mounted_ = true;

  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      name_, file_system_type_, storage::FileSystemMountOption(),
      temp_dir_.GetPath());
  ash::FileSystemBackend::Get(
      *file_manager::util::GetFileManagerFileSystemContext(profile))
      ->GrantFileAccessToOrigin(file_manager::util::GetFilesAppOrigin(),
                                base::FilePath(name_));
  if (file_manager::VolumeManager::Get(profile_)) {
    file_manager::VolumeManager::Get(profile_)->AddVolumeForTesting(  // IN-TEST
        temp_dir_.GetPath(), volume_type_, DeviceType::kUnknown,
        /*read_only=*/false);
  }
}

bool ScopedTestMountPoint::IsValid() const {
  return temp_dir_.IsValid();
}

base::FilePath ScopedTestMountPoint::CreateFile(
    const base::FilePath& relative_path,
    const std::string& content) {
  const base::FilePath path = GetRootPath().Append(relative_path);
  if (!base::CreateDirectory(path.DirName()))
    return base::FilePath();
  if (!base::WriteFile(path, content))
    return base::FilePath();
  return path;
}

base::FilePath ScopedTestMountPoint::CreateArbitraryFile() {
  return CreateFile(base::FilePath(base::UnguessableToken::Create().ToString()),
                    /*content=*/std::string());
}

}  // namespace holding_space
}  // namespace ash

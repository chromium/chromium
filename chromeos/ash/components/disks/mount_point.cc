// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/disks/mount_point.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"

namespace ash {
namespace disks {
namespace {

void OnMountDone(DiskMountManager* disk_mount_manager,
                 MountPoint::DoneCallback callback,
                 MountError error_code,
                 const DiskMountManager::MountPoint& mount_info) {
  std::unique_ptr<MountPoint> mount_point;
  if (error_code == MountError::kSuccess) {
    DCHECK(!mount_info.mount_path.empty());
    mount_point = std::make_unique<MountPoint>(
        base::FilePath(mount_info.mount_path), disk_mount_manager);
  }

  // Post a task to guarantee the callback isn't called inline with the
  // Mount() call.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), error_code, std::move(mount_point)));
}

}  // namespace

// static
void MountPoint::Mount(DiskMountManager* disk_mount_manager,
                       const std::string& source_path,
                       const std::string& source_format,
                       const std::string& mount_label,
                       const std::vector<std::string>& mount_options,
                       MountType mount_type,
                       MountAccessMode access_mode,
                       DoneCallback callback) {
  // |disk_mount_manager| owns the callback to OnMountDone, so we can bind it as
  // unretained.
  disk_mount_manager->MountPath(
      source_path, source_format, mount_label, mount_options, mount_type,
      access_mode,
      base::BindOnce(&OnMountDone, base::Unretained(disk_mount_manager),
                     std::move(callback)));
}

MountPoint::MountPoint(const base::FilePath& mount_path,
                       DiskMountManager* disk_mount_manager)
    : mount_path_(mount_path), disk_mount_manager_(disk_mount_manager) {
  DCHECK(!mount_path_.empty());
}

MountPoint::~MountPoint() {
  if (!mount_path_.empty()) {
    disk_mount_manager_->UnmountPath(
        mount_path_.value(), base::BindOnce([](MountError error_code) {
          LOG_IF(WARNING, error_code != MountError::kSuccess)
              << "Failed to unmount with error code: " << error_code;
        }));
  }
}

void MountPoint::Unmount(MountPoint::UnmountCallback callback) {
  DCHECK(callback);
  DCHECK(!mount_path_.empty());

  // Make a copy of the |mount_path_| on the stack and clear it, in case the
  // callback runs inline and deletes |this|.
  const std::string mount_path = mount_path_.value();
  mount_path_.clear();
  disk_mount_manager_->UnmountPath(
      mount_path,
      base::BindOnce(&MountPoint::OnUmountDone, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void MountPoint::OnUmountDone(MountPoint::UnmountCallback callback,
                              MountError unmount_error) {
  std::move(callback).Run(unmount_error);
}

}  // namespace disks
}  // namespace ash

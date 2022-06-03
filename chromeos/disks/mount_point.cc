// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/disks/mount_point.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace chromeos {
namespace disks {
namespace {

class MountWatcher : public DiskMountManager::Observer {
 public:
  MountWatcher() = delete;
  MountWatcher(const MountWatcher&) = delete;
  MountWatcher& operator=(const MountWatcher&) = delete;

  MountWatcher(DiskMountManager* disk_mount_manager,
               const std::string& source_path,
               MountType mount_type,
               MountPoint::DoneCallback callback)
      : disk_mount_manager_(disk_mount_manager),
        source_path_(source_path),
        mount_type_(mount_type),
        callback_(std::move(callback)) {
    DCHECK(callback_);
    disk_mount_manager_->AddObserver(this);
  }

  ~MountWatcher() override { disk_mount_manager_->RemoveObserver(this); }

 private:
  // DiskMountManager::Observer overrides.
  void OnMountEvent(
      DiskMountManager::MountEvent event,
      MountError error_code,
      const DiskMountManager::MountPointInfo& mount_info) override {
    if (mount_info.mount_type != mount_type_ ||
        mount_info.source_path != source_path_ ||
        event != chromeos::disks::DiskMountManager::MOUNTING) {
      return;
    }

    DCHECK(callback_);
    std::unique_ptr<MountPoint> mount_point;
    if (error_code == chromeos::MOUNT_ERROR_NONE) {
      DCHECK(!mount_info.mount_path.empty());
      mount_point = std::make_unique<MountPoint>(
          base::FilePath(mount_info.mount_path), disk_mount_manager_);
    }

    // Post a task to guarantee the callback isn't called inline with the
    // Mount() call.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), error_code,
                                  std::move(mount_point)));

    delete this;
  }

  DiskMountManager* const disk_mount_manager_;
  const std::string source_path_;
  const MountType mount_type_;
  MountPoint::DoneCallback callback_;
};

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
  // MountWatcher needs to be created before mounting because MountPath() may
  // signal a result inline.
  // Note: MountWatcher owns itself.
  new MountWatcher(disk_mount_manager, source_path, mount_type,
                   std::move(callback));

  disk_mount_manager->MountPath(source_path, source_format, mount_label,
                                mount_options, mount_type, access_mode);
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
          LOG_IF(WARNING, error_code != MOUNT_ERROR_NONE)
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
}  // namespace chromeos

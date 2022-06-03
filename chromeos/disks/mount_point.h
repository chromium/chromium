// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISKS_MOUNT_POINT_H_
#define CHROMEOS_DISKS_MOUNT_POINT_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace chromeos {
namespace disks {

class DiskMountManager;

// MountPoint is a thin wrapper around a mount point that was mounted with
// DiskMountManager. MountPoint 'owns' the mount point and unmounts it on
// destruction.
class COMPONENT_EXPORT(CHROMEOS_DISKS) MountPoint {
 public:
  using DoneCallback =
      base::OnceCallback<void(MountError, std::unique_ptr<MountPoint>)>;
  using UnmountCallback = DiskMountManager::UnmountPathCallback;

  // Mounts a device, archive, or network filesystem, and runs |callback| when
  // done. |callback| will never be called inline. |callback| should be bound
  // with a WeakPtr<> since Mount() can take an indefinite amount of time.
  // See DiskMountManager::MountPath() for other argument details.
  static void Mount(DiskMountManager* disk_mount_manager,
                    const std::string& source_path,
                    const std::string& source_format,
                    const std::string& mount_label,
                    const std::vector<std::string>& mount_options,
                    MountType mount_type,
                    MountAccessMode access_mode,
                    DoneCallback callback);

  MountPoint() = delete;
  MountPoint(const MountPoint&) = delete;
  MountPoint& operator=(const MountPoint&) = delete;

  MountPoint(const base::FilePath& mount_path,
             DiskMountManager* disk_mount_manager);
  ~MountPoint();

  // Unmounts the mount point, and runs |callback| when done. |callback| must be
  // non-null, and will not be run if |this| is destroyed before the unmount has
  // completed.
  void Unmount(UnmountCallback callback);

  const base::FilePath& mount_path() const { return mount_path_; }

 private:
  // Callback for DiskMountManager::UnmountPath().
  void OnUmountDone(UnmountCallback callback, MountError unmount_error);

  base::FilePath mount_path_;
  DiskMountManager* const disk_mount_manager_;

  base::WeakPtrFactory<MountPoint> weak_factory_{this};
};

}  // namespace disks
}  // namespace chromeos

#endif  // CHROMEOS_DISKS_MOUNT_POINT_H_

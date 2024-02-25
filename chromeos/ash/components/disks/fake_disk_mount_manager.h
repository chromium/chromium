// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DISKS_FAKE_DISK_MOUNT_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DISKS_FAKE_DISK_MOUNT_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"

namespace ash::disks {

class FakeDiskMountManager : public DiskMountManager {
 public:
  struct MountRequest {
    MountRequest(const std::string& source_path,
                 const std::string& source_format,
                 const std::string& mount_label,
                 const std::vector<std::string>& mount_options,
                 MountType type,
                 MountAccessMode access_mode);
    MountRequest(const MountRequest& other);
    ~MountRequest();

    std::string source_path;
    std::string source_format;
    std::string mount_label;
    std::vector<std::string> mount_options;
    MountType type;
    MountAccessMode access_mode;
  };

  struct RemountAllRequest {
    explicit RemountAllRequest(MountAccessMode access_mode);
    MountAccessMode access_mode;
  };

  FakeDiskMountManager();

  FakeDiskMountManager(const FakeDiskMountManager&) = delete;
  FakeDiskMountManager& operator=(const FakeDiskMountManager&) = delete;

  ~FakeDiskMountManager() override;

  const std::vector<MountRequest>& mount_requests() const {
    return mount_requests_;
  }
  const std::vector<std::string>& unmount_requests() const {
    return unmount_requests_;
  }
  const std::vector<RemountAllRequest>& remount_all_requests() const {
    return remount_all_requests_;
  }

  // Emulates that all mount request finished.
  // Return true if there was one or more mount request enqueued, or false
  // otherwise.
  bool FinishAllUnmountPathRequests();

  // Fails a future unmount request for |mount_path| with |error_code|.
  void FailUnmountRequest(const std::string& mount_path, MountError error_code);

  // DiskMountManager overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  const Disks& disks() const override;
  const Disk* FindDiskBySourcePath(
      const std::string& source_path) const override;
  const MountPoints& mount_points() const override;
  void EnsureMountInfoRefreshed(EnsureMountInfoRefreshedCallback callback,
                                bool force) override;
  void MountPath(const std::string& source_path,
                 const std::string& source_format,
                 const std::string& mount_label,
                 const std::vector<std::string>& mount_options,
                 MountType type,
                 MountAccessMode access_mode,
                 MountPathCallback) override;
  // In order to simulate asynchronous invocation of callbacks after unmount
  // is finished, |callback| will be invoked only when
  // |FinishAllUnmountRequest()| is called.
  void UnmountPath(const std::string& mount_path,
                   UnmountPathCallback callback) override;
  void RemountAllRemovableDrives(MountAccessMode access_mode) override;
  void FormatMountedDevice(const std::string& mount_path,
                           FormatFileSystemType filesystem,
                           const std::string& label) override;
  void SinglePartitionFormatDevice(const std::string& device_path,
                                   FormatFileSystemType filesystem,
                                   const std::string& label) override;
  void RenameMountedDevice(const std::string& mount_path,
                           const std::string& volume_name) override;
  void UnmountDeviceRecursively(
      const std::string& device_path,
      UnmountDeviceRecursivelyCallbackType callback) override;

  bool AddDiskForTest(std::unique_ptr<Disk> disk) override;
  bool AddMountPointForTest(const MountPoint& mount_point) override;
  void InvokeDiskEventForTest(DiskEvent event, const Disk* disk);

  void RegisterMountPointForNetworkStorageScheme(const std::string& scheme,
                                                 const std::string& mount_path);

 private:
  base::ObserverList<Observer> observers_;
  base::queue<base::OnceClosure> pending_unmount_callbacks_;

  Disks disks_;
  MountPoints mount_points_;

  std::vector<MountRequest> mount_requests_;
  std::vector<std::string> unmount_requests_;
  std::vector<RemountAllRequest> remount_all_requests_;
  std::map<std::string, MountError> unmount_errors_;
  // Maps a network storage URL scheme to a registered mount point path for that
  // scheme.
  std::map<std::string, std::string> network_storage_mount_paths_;
};

}  // namespace ash::disks

#endif  // CHROMEOS_ASH_COMPONENTS_DISKS_FAKE_DISK_MOUNT_MANAGER_H_

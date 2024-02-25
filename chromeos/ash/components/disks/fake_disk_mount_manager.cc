// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "chromeos/ash/components/disks/disk.h"

namespace ash::disks {

FakeDiskMountManager::MountRequest::MountRequest(
    const std::string& source_path,
    const std::string& source_format,
    const std::string& mount_label,
    const std::vector<std::string>& mount_options,
    MountType type,
    MountAccessMode access_mode)
    : source_path(source_path),
      source_format(source_format),
      mount_label(mount_label),
      mount_options(mount_options),
      type(type),
      access_mode(access_mode) {}

FakeDiskMountManager::MountRequest::MountRequest(const MountRequest& other) =
    default;

FakeDiskMountManager::MountRequest::~MountRequest() = default;

FakeDiskMountManager::RemountAllRequest::RemountAllRequest(
    MountAccessMode access_mode)
    : access_mode(access_mode) {}

FakeDiskMountManager::FakeDiskMountManager() = default;

FakeDiskMountManager::~FakeDiskMountManager() = default;

void FakeDiskMountManager::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void FakeDiskMountManager::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

const DiskMountManager::Disks& FakeDiskMountManager::disks() const {
  return disks_;
}

const Disk* FakeDiskMountManager::FindDiskBySourcePath(
    const std::string& source_path) const {
  Disks::const_iterator iter = disks_.find(source_path);
  return iter != disks_.end() ? iter->get() : nullptr;
}

const DiskMountManager::MountPoints& FakeDiskMountManager::mount_points()
    const {
  return mount_points_;
}

void FakeDiskMountManager::EnsureMountInfoRefreshed(
    EnsureMountInfoRefreshedCallback callback,
    bool force) {
  std::move(callback).Run(true);
}

void FakeDiskMountManager::MountPath(
    const std::string& source_path,
    const std::string& source_format,
    const std::string& mount_label,
    const std::vector<std::string>& mount_options,
    MountType type,
    MountAccessMode access_mode,
    MountPathCallback callback) {
  mount_requests_.emplace_back(source_path, source_format, mount_label,
                               mount_options, type, access_mode);

  std::string mount_path = source_path;

  if (type == MountType::kNetworkStorage) {
    // Split the source path into components, first of which would be the URL
    // scheme.
    std::vector<std::string> source_components = base::SplitStringUsingSubstr(
        source_path, "://", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (source_components.size() > 1u) {
      const auto registered_mount_path =
          network_storage_mount_paths_.find(source_components[0]);
      if (registered_mount_path != network_storage_mount_paths_.end()) {
        mount_path = registered_mount_path->second;
      }
    }
  }

  const MountPoint mount_point{source_path, mount_path, type};
  mount_points_.insert(mount_point);
  std::move(callback).Run(MountError::kSuccess, mount_point);
  for (auto& observer : observers_) {
    observer.OnMountEvent(DiskMountManager::MOUNTING, MountError::kSuccess,
                          mount_point);
  }
}

void FakeDiskMountManager::UnmountPath(const std::string& mount_path,
                                       UnmountPathCallback callback) {
  unmount_requests_.emplace_back(mount_path);

  MountError error = MountError::kSuccess;
  auto unmount_iter = unmount_errors_.find(mount_path);
  if (unmount_iter != unmount_errors_.end()) {
    error = unmount_iter->second;
    unmount_errors_.erase(unmount_iter);
  } else {
    MountPoints::iterator iter = mount_points_.find(mount_path);
    if (iter == mount_points_.end()) {
      return;
    }

    const MountPoint mount_point = *iter;
    mount_points_.erase(iter);
    for (auto& observer : observers_) {
      observer.OnMountEvent(DiskMountManager::UNMOUNTING, MountError::kSuccess,
                            mount_point);
    }
  }

  // Enqueue callback so that |FakeDiskMountManager::FinishAllUnmountRequest()|
  // can call them.
  if (callback) {
    // Some tests pass a null |callback|.
    pending_unmount_callbacks_.push(base::BindOnce(std::move(callback), error));
  }
}

void FakeDiskMountManager::RemountAllRemovableDrives(
    MountAccessMode access_mode) {
  remount_all_requests_.emplace_back(access_mode);
}

bool FakeDiskMountManager::FinishAllUnmountPathRequests() {
  if (pending_unmount_callbacks_.empty()) {
    return false;
  }

  while (!pending_unmount_callbacks_.empty()) {
    std::move(pending_unmount_callbacks_.front()).Run();
    pending_unmount_callbacks_.pop();
  }
  return true;
}

void FakeDiskMountManager::FailUnmountRequest(const std::string& mount_path,
                                              MountError error_code) {
  unmount_errors_[mount_path] = error_code;
}

void FakeDiskMountManager::FormatMountedDevice(const std::string& mount_path,
                                               FormatFileSystemType filesystem,
                                               const std::string& label) {}

void FakeDiskMountManager::SinglePartitionFormatDevice(
    const std::string& device_path,
    FormatFileSystemType filesystem,
    const std::string& label) {}

void FakeDiskMountManager::RenameMountedDevice(const std::string& mount_path,
                                               const std::string& volume_name) {
}

void FakeDiskMountManager::UnmountDeviceRecursively(
    const std::string& device_path,
    UnmountDeviceRecursivelyCallbackType callback) {}

bool FakeDiskMountManager::AddDiskForTest(std::unique_ptr<Disk> disk) {
  DCHECK(disk);
  return disks_.insert(std::move(disk)).second;
}

bool FakeDiskMountManager::AddMountPointForTest(const MountPoint& mount_point) {
  if (mount_point.mount_type == MountType::kDevice &&
      !base::Contains(disks_, mount_point.source_path)) {
    // Device mount point must have a disk entry.
    return false;
  }

  mount_points_.insert(mount_point);
  return true;
}

void FakeDiskMountManager::InvokeDiskEventForTest(
    DiskMountManager::DiskEvent event,
    const Disk* disk) {
  for (auto& observer : observers_) {
    disk->is_auto_mountable() ? observer.OnAutoMountableDiskEvent(event, *disk)
                              : observer.OnBootDeviceDiskEvent(event, *disk);
  }
}

void FakeDiskMountManager::RegisterMountPointForNetworkStorageScheme(
    const std::string& scheme,
    const std::string& mount_path) {
  network_storage_mount_paths_.emplace(scheme, mount_path);
}

}  // namespace ash::disks

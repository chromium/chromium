// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "chromeos/ash/components/disks/disk.h"

namespace ash {
namespace disks {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::ReturnRef;

}  // namespace

void MockDiskMountManager::AddObserver(DiskMountManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void MockDiskMountManager::RemoveObserver(
    DiskMountManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

MockDiskMountManager::MockDiskMountManager() {
  ON_CALL(*this, disks())
      .WillByDefault(Invoke(this, &MockDiskMountManager::disksInternal));
  ON_CALL(*this, mount_points())
      .WillByDefault(Invoke(this, &MockDiskMountManager::mountPointsInternal));
  ON_CALL(*this, FindDiskBySourcePath(_))
      .WillByDefault(
          Invoke(this, &MockDiskMountManager::FindDiskBySourcePathInternal));
  // Invoke doesn't handle move-only types, so use a lambda instead.
  ON_CALL(*this, EnsureMountInfoRefreshed(_, _))
      .WillByDefault([](EnsureMountInfoRefreshedCallback callback, bool force) {
        std::move(callback).Run(true);
      });
}

MockDiskMountManager::~MockDiskMountManager() = default;

void MockDiskMountManager::NotifyMountEvent(MountEvent event,
                                            MountError error_code,
                                            const MountPoint& mount_info) {
  for (auto& observer : observers_)
    observer.OnMountEvent(event, error_code, mount_info);
}

void MockDiskMountManager::SetupDefaultReplies() {
  EXPECT_CALL(*this, disks()).WillRepeatedly(ReturnRef(disks_));
  EXPECT_CALL(*this, mount_points()).WillRepeatedly(ReturnRef(mount_points_));
  EXPECT_CALL(*this, FindDiskBySourcePath(_)).Times(AnyNumber());
  EXPECT_CALL(*this, EnsureMountInfoRefreshed(_, _)).Times(AnyNumber());
  EXPECT_CALL(*this, MountPath(_, _, _, _, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(*this, UnmountPath(_, _)).Times(AnyNumber());
  EXPECT_CALL(*this, RemountAllRemovableDrives(_)).Times(AnyNumber());
  EXPECT_CALL(*this, FormatMountedDevice(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*this, SinglePartitionFormatDevice(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*this, UnmountDeviceRecursively(_, _)).Times(AnyNumber());
}

void MockDiskMountManager::CreateDiskEntryForMountDevice(
    std::unique_ptr<Disk> disk) {
  disks_.insert(std::move(disk));
}

void MockDiskMountManager::CreateDiskEntryForMountDevice(
    const DiskMountManager::MountPoint& mount_info,
    const std::string& device_id,
    const std::string& device_label,
    const std::string& vendor_name,
    const std::string& product_name,
    DeviceType device_type,
    uint64_t total_size_in_bytes,
    bool is_parent,
    bool has_media,
    bool on_boot_device,
    bool on_removable_device,
    const std::string& file_system_type) {
  std::unique_ptr<Disk> disk_ptr =
      Disk::Builder()
          .SetDevicePath(mount_info.source_path)
          .SetMountPath(mount_info.mount_path)
          .SetFilePath(mount_info.source_path)
          .SetDeviceLabel(device_label)
          .SetVendorName(vendor_name)
          .SetProductName(product_name)
          .SetFileSystemUUID(device_id)
          .SetDeviceType(device_type)
          .SetSizeInBytes(total_size_in_bytes)
          .SetIsParent(is_parent)
          .SetHasMedia(has_media)
          .SetOnBootDevice(on_boot_device)
          .SetOnRemovableDevice(on_removable_device)
          .SetFileSystemType(file_system_type)
          .Build();
  CreateDiskEntryForMountDevice(std::move(disk_ptr));
}

void MockDiskMountManager::RemoveDiskEntryForMountDevice(
    const DiskMountManager::MountPoint& mount_info) {
  const auto it = disks_.find(mount_info.source_path);
  CHECK(it != disks_.end()) << "Cannot find " << mount_info.source_path;
  disks_.erase(it);
}

const DiskMountManager::MountPoints& MockDiskMountManager::mountPointsInternal()
    const {
  return mount_points_;
}

const Disk* MockDiskMountManager::FindDiskBySourcePathInternal(
    const std::string& source_path) const {
  Disks::const_iterator disk_it = disks_.find(source_path);
  return disk_it == disks_.end() ? nullptr : disk_it->get();
}

}  // namespace disks
}  // namespace ash

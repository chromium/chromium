// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/disks/mock_disk_mount_manager.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/strings/string_util.h"
#include "chromeos/disks/disk.h"

using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::ReturnRef;

namespace chromeos {
namespace disks {

namespace {

const char kTestSystemPath[] = "/this/system/path";
const char kTestStorageDevicePath[] = "/this/system";
const char kTestDevicePath[] = "/this/device/path";
const char kTestMountPath[] = "/media/foofoo";
const char kTestFilePath[] = "/this/file/path";
const char kTestDeviceLabel[] = "A label";
const char kTestDriveLabel[] = "Another label";
const char kTestVendorId[] = "0123";
const char kTestVendorName[] = "A vendor";
const char kTestProductId[] = "abcd";
const char kTestProductName[] = "A product";
const char kTestUuid[] = "FFFF-FFFF";
const char kTestFileSystemType[] = "vfat";

std::unique_ptr<Disk::Builder> MakeDiskBuilder() {
  std::unique_ptr<Disk::Builder> builder = std::make_unique<Disk::Builder>();
  builder->SetDevicePath(kTestDevicePath)
      .SetFilePath(kTestFilePath)
      .SetDriveLabel(kTestDriveLabel)
      .SetVendorId(kTestVendorId)
      .SetVendorName(kTestVendorName)
      .SetProductId(kTestProductId)
      .SetProductName(kTestProductName)
      .SetFileSystemUUID(kTestUuid)
      .SetStorageDevicePath(kTestStorageDevicePath)
      .SetHasMedia(true)
      .SetOnRemovableDevice(true)
      .SetFileSystemType(kTestFileSystemType);
  return builder;
}

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
      .WillByDefault(Invoke(
          this, &MockDiskMountManager::FindDiskBySourcePathInternal));
  // Invoke doesn't handle move-only types, so use a lambda instead.
  ON_CALL(*this, EnsureMountInfoRefreshed(_, _))
      .WillByDefault([](EnsureMountInfoRefreshedCallback callback, bool force) {
        std::move(callback).Run(true);
      });
}

MockDiskMountManager::~MockDiskMountManager() = default;

void MockDiskMountManager::NotifyDeviceInsertEvents() {
  std::unique_ptr<Disk> disk1_ptr = MakeDiskBuilder()
                                        ->SetDeviceType(DEVICE_TYPE_USB)
                                        .SetSizeInBytes(4294967295U)
                                        .Build();
  Disk* disk1 = disk1_ptr.get();

  disks_.clear();
  disks_[std::string(kTestDevicePath)] = std::move(disk1_ptr);

  // Device Added
  NotifyDeviceChanged(DEVICE_ADDED, kTestSystemPath);

  // Disk Added
  NotifyDiskChanged(DISK_ADDED, disk1);

  // Disk Changed
  std::unique_ptr<Disk> disk2_ptr = MakeDiskBuilder()
                                        ->SetMountPath(kTestMountPath)
                                        .SetDeviceType(DEVICE_TYPE_MOBILE)
                                        .SetSizeInBytes(1073741824)
                                        .Build();
  Disk* disk2 = disk2_ptr.get();
  disks_.clear();
  disks_[std::string(kTestDevicePath)] = std::move(disk2_ptr);
  NotifyDiskChanged(DISK_CHANGED, disk2);
}

void MockDiskMountManager::NotifyDeviceRemoveEvents() {
  std::unique_ptr<Disk> disk_ptr = MakeDiskBuilder()
                                       ->SetMountPath(kTestMountPath)
                                       .SetDeviceLabel(kTestDeviceLabel)
                                       .SetDeviceType(DEVICE_TYPE_SD)
                                       .SetSizeInBytes(1073741824)
                                       .Build();
  Disk* disk = disk_ptr.get();
  disks_.clear();
  disks_[std::string(kTestDevicePath)] = std::move(disk_ptr);
  NotifyDiskChanged(DISK_REMOVED, disk);
}

void MockDiskMountManager::NotifyMountEvent(MountEvent event,
                                            MountError error_code,
                                            const MountPointInfo& mount_info) {
  for (auto& observer : observers_)
    observer.OnMountEvent(event, error_code, mount_info);
}

void MockDiskMountManager::SetupDefaultReplies() {
  EXPECT_CALL(*this, disks())
      .WillRepeatedly(ReturnRef(disks_));
  EXPECT_CALL(*this, mount_points())
      .WillRepeatedly(ReturnRef(mount_points_));
  EXPECT_CALL(*this, FindDiskBySourcePath(_))
      .Times(AnyNumber());
  EXPECT_CALL(*this, EnsureMountInfoRefreshed(_, _)).Times(AnyNumber());
  EXPECT_CALL(*this, MountPath(_, _, _, _, _, _)).Times(AnyNumber());
  EXPECT_CALL(*this, UnmountPath(_, _)).Times(AnyNumber());
  EXPECT_CALL(*this, RemountAllRemovableDrives(_)).Times(AnyNumber());
  EXPECT_CALL(*this, FormatMountedDevice(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*this, UnmountDeviceRecursively(_, _))
      .Times(AnyNumber());
}

void MockDiskMountManager::CreateDiskEntryForMountDevice(
    const DiskMountManager::MountPointInfo& mount_info,
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
  disks_[std::string(mount_info.source_path)] = std::move(disk_ptr);
}

void MockDiskMountManager::RemoveDiskEntryForMountDevice(
    const DiskMountManager::MountPointInfo& mount_info) {
  disks_.erase(mount_info.source_path);
}

const DiskMountManager::MountPointMap&
MockDiskMountManager::mountPointsInternal() const {
  return mount_points_;
}

const Disk* MockDiskMountManager::FindDiskBySourcePathInternal(
    const std::string& source_path) const {
  DiskMap::const_iterator disk_it = disks_.find(source_path);
  return disk_it == disks_.end() ? nullptr : disk_it->second.get();
}

void MockDiskMountManager::NotifyDiskChanged(DiskEvent event,
                                             const Disk* disk) {
  for (auto& observer : observers_) {
    disk->is_auto_mountable() ? observer.OnAutoMountableDiskEvent(event, *disk)
                              : observer.OnBootDeviceDiskEvent(event, *disk);
  }
}

void MockDiskMountManager::NotifyDeviceChanged(DeviceEvent event,
                                               const std::string& path) {
  for (auto& observer : observers_)
    observer.OnDeviceEvent(event, path);
}

}  // namespace disks
}  // namespace chromeos

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "chromeos/disks/suspend_unmount_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace disks {
namespace {

const char kDeviceId[] = "device_id";
const char kDeviceLabel[] = "device_label";
const char kVendor[] = "vendor";
const char kProduct[] = "product";
const char kFileSystemType[] = "exfat";

class FakeDiskMountManager : public MockDiskMountManager {
 public:
  void NotifyUnmountDeviceComplete(MountError error) {
    for (size_t i = 0; i < callbacks_.size(); i++) {
      std::move(callbacks_[i]).Run(error);
    }
    callbacks_.clear();
  }

  const std::vector<std::string>& unmounting_mount_paths() const {
    return unmounting_mount_paths_;
  }

 private:
  void UnmountPath(const std::string& mount_path,
                   UnmountPathCallback callback) override {
    unmounting_mount_paths_.push_back(mount_path);
    callbacks_.push_back(std::move(callback));
  }
  std::vector<std::string> unmounting_mount_paths_;
  std::vector<UnmountPathCallback> callbacks_;
};

class SuspendUnmountManagerTest : public testing::Test {
 public:
  SuspendUnmountManagerTest() {
    PowerManagerClient::InitializeFake();
    suspend_unmount_manager_ =
        std::make_unique<SuspendUnmountManager>(&disk_mount_manager_);
  }

  ~SuspendUnmountManagerTest() override {
    suspend_unmount_manager_.reset();
    PowerManagerClient::Shutdown();
  }

 protected:
  FakeDiskMountManager disk_mount_manager_;
  std::unique_ptr<SuspendUnmountManager> suspend_unmount_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuspendUnmountManagerTest);
};

TEST_F(SuspendUnmountManagerTest, Basic) {
  const std::string kDummyMountPathUsb = "/dummy/mount/usb";
  const std::string kDummyMountPathSd = "/dummy/mount/sd";
  const std::string kDummyMountPathUnknown = "/dummy/mount/unknown";
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/dummy/device/usb", kDummyMountPathUsb, chromeos::MOUNT_TYPE_DEVICE,
          chromeos::disks::MOUNT_CONDITION_NONE),
      kDeviceId, kDeviceLabel, kVendor, kProduct, chromeos::DEVICE_TYPE_USB,
      1024 * 1024, false /* is_parent */, false /* has_media */,
      false /* on_boot_device */, true /* on_removable_device */,
      kFileSystemType);
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/dummy/device/sd", kDummyMountPathSd, chromeos::MOUNT_TYPE_DEVICE,
          chromeos::disks::MOUNT_CONDITION_NONE),
      kDeviceId, kDeviceLabel, kVendor, kProduct, chromeos::DEVICE_TYPE_SD,
      1024 * 1024, false /* is_parent */, false /* has_media */,
      false /* on_boot_device */, true /* on_removable_device */,
      kFileSystemType);
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/dummy/device/unknown", kDummyMountPathUnknown,
          chromeos::MOUNT_TYPE_DEVICE, chromeos::disks::MOUNT_CONDITION_NONE),
      kDeviceId, kDeviceLabel, kVendor, kProduct, chromeos::DEVICE_TYPE_UNKNOWN,
      1024 * 1024, false /* is_parent */, false /* has_media */,
      false /* on_boot_device */, true /* on_removable_device */,
      kFileSystemType);
  disk_mount_manager_.SetupDefaultReplies();
  FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_EQ(
      1,
      FakePowerManagerClient::Get()->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(2u, disk_mount_manager_.unmounting_mount_paths().size());
  EXPECT_EQ(1, std::count(disk_mount_manager_.unmounting_mount_paths().begin(),
                          disk_mount_manager_.unmounting_mount_paths().end(),
                          kDummyMountPathUsb));
  EXPECT_EQ(1, std::count(disk_mount_manager_.unmounting_mount_paths().begin(),
                          disk_mount_manager_.unmounting_mount_paths().end(),
                          kDummyMountPathSd));
  EXPECT_EQ(0, std::count(disk_mount_manager_.unmounting_mount_paths().begin(),
                          disk_mount_manager_.unmounting_mount_paths().end(),
                          kDummyMountPathUnknown));
  disk_mount_manager_.NotifyUnmountDeviceComplete(MOUNT_ERROR_NONE);
  EXPECT_EQ(
      0,
      FakePowerManagerClient::Get()->num_pending_suspend_readiness_callbacks());
}

TEST_F(SuspendUnmountManagerTest, CancelAndSuspendAgain) {
  const std::string kDummyMountPath = "/dummy/mount";
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      chromeos::disks::DiskMountManager::MountPointInfo(
          "/dummy/device", kDummyMountPath, chromeos::MOUNT_TYPE_DEVICE,
          chromeos::disks::MOUNT_CONDITION_NONE),
      kDeviceId, kDeviceLabel, kVendor, kProduct, chromeos::DEVICE_TYPE_USB,
      1024 * 1024, false /* is_parent */, false /* has_media */,
      false /* on_boot_device */, true /* on_removable_device */,
      kFileSystemType);
  disk_mount_manager_.SetupDefaultReplies();
  FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(
      1,
      FakePowerManagerClient::Get()->num_pending_suspend_readiness_callbacks());
  ASSERT_EQ(1u, disk_mount_manager_.unmounting_mount_paths().size());
  EXPECT_EQ(kDummyMountPath,
            disk_mount_manager_.unmounting_mount_paths().front());

  // Suspend cancelled.
  FakePowerManagerClient::Get()->SendSuspendDone();

  // Suspend again.
  FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  ASSERT_EQ(2u, disk_mount_manager_.unmounting_mount_paths().size());
  EXPECT_EQ(kDummyMountPath,
            disk_mount_manager_.unmounting_mount_paths().front());
}

}  // namespace
}  // namespace disks
}  // namespace chromeos

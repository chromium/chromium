// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "chromeos/ash/components/disks/suspend_unmount_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
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
    chromeos::PowerManagerClient::InitializeFake();
    suspend_unmount_manager_ =
        std::make_unique<SuspendUnmountManager>(&disk_mount_manager_);
  }

  SuspendUnmountManagerTest(const SuspendUnmountManagerTest&) = delete;
  SuspendUnmountManagerTest& operator=(const SuspendUnmountManagerTest&) =
      delete;

  ~SuspendUnmountManagerTest() override {
    suspend_unmount_manager_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  FakeDiskMountManager disk_mount_manager_;
  std::unique_ptr<SuspendUnmountManager> suspend_unmount_manager_;
};

TEST_F(SuspendUnmountManagerTest, Basic) {
  const std::string kDummyMountPathUsb = "/dummy/mount/usb";
  const std::string kDummyMountPathSd = "/dummy/mount/sd";
  const std::string kDummyMountPathUnknown = "/dummy/mount/unknown";
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      {"/dummy/device/usb", kDummyMountPathUsb, MountType::kDevice}, kDeviceId,
      kDeviceLabel, kVendor, kProduct, DeviceType::kUSB, 1024 * 1024,
      false /* is_parent */, false /* has_media */, false /* on_boot_device */,
      true /* on_removable_device */, kFileSystemType);
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      {"/dummy/device/sd", kDummyMountPathSd, MountType::kDevice}, kDeviceId,
      kDeviceLabel, kVendor, kProduct, DeviceType::kSD, 1024 * 1024,
      false /* is_parent */, false /* has_media */, false /* on_boot_device */,
      true /* on_removable_device */, kFileSystemType);
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      {"/dummy/device/unknown", kDummyMountPathUnknown, MountType::kDevice},
      kDeviceId, kDeviceLabel, kVendor, kProduct, DeviceType::kUnknown,
      1024 * 1024, false /* is_parent */, false /* has_media */,
      false /* on_boot_device */, true /* on_removable_device */,
      kFileSystemType);
  disk_mount_manager_.SetupDefaultReplies();
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());
  EXPECT_EQ(2u, disk_mount_manager_.unmounting_mount_paths().size());
  EXPECT_EQ(1, base::ranges::count(disk_mount_manager_.unmounting_mount_paths(),
                                   kDummyMountPathUsb));
  EXPECT_EQ(1, base::ranges::count(disk_mount_manager_.unmounting_mount_paths(),
                                   kDummyMountPathSd));
  EXPECT_EQ(0, base::ranges::count(disk_mount_manager_.unmounting_mount_paths(),
                                   kDummyMountPathUnknown));
  disk_mount_manager_.NotifyUnmountDeviceComplete(MountError::kSuccess);
  EXPECT_EQ(0, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());
}

TEST_F(SuspendUnmountManagerTest, CancelAndSuspendAgain) {
  const std::string kDummyMountPath = "/dummy/mount";
  disk_mount_manager_.CreateDiskEntryForMountDevice(
      {"/dummy/device", kDummyMountPath, MountType::kDevice}, kDeviceId,
      kDeviceLabel, kVendor, kProduct, DeviceType::kUSB, 1024 * 1024,
      false /* is_parent */, false /* has_media */, false /* on_boot_device */,
      true /* on_removable_device */, kFileSystemType);
  disk_mount_manager_.SetupDefaultReplies();
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  EXPECT_EQ(1, chromeos::FakePowerManagerClient::Get()
                   ->num_pending_suspend_readiness_callbacks());
  ASSERT_EQ(1u, disk_mount_manager_.unmounting_mount_paths().size());
  EXPECT_EQ(kDummyMountPath,
            disk_mount_manager_.unmounting_mount_paths().front());

  // Suspend cancelled.
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();

  // Suspend again.
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
  ASSERT_EQ(2u, disk_mount_manager_.unmounting_mount_paths().size());
  EXPECT_EQ(kDummyMountPath,
            disk_mount_manager_.unmounting_mount_paths().front());
}

}  // namespace
}  // namespace disks
}  // namespace ash

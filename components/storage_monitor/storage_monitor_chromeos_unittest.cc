// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_monitor_chromeos.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/disks/disk.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "components/storage_monitor/mock_removable_storage_observer.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/test_media_transfer_protocol_manager_chromeos.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage_monitor {

namespace {

using chromeos::disks::Disk;
using chromeos::disks::DiskMountManager;
using testing::_;

const char kDevice1[] = "/dev/d1";
const char kDevice1Name[] = "d1";
const char kDevice2[] = "/dev/disk/d2";
const char kDevice2Name[] = "d2";
const char kEmptyDeviceLabel[] = "";
const char kMountPointA[] = "mnt_a";
const char kMountPointB[] = "mnt_b";
const char kSDCardDeviceName1[] = "8.6 MB Amy_SD";
const char kSDCardDeviceName2[] = "8.6 MB SD Card";
const char kSDCardMountPoint1[] = "media/removable/Amy_SD";
const char kSDCardMountPoint2[] = "media/removable/SD Card";
const char kProductName[] = "Z101";
const char kUniqueId1[] = "FFFF-FFFF";
const char kUniqueId2[] = "FFFF-FF0F";
const char kVendorName[] = "CompanyA";
const char kFileSystemType[] = "exfat";

uint64_t kDevice1SizeInBytes = 113048;
uint64_t kDevice2SizeInBytes = 212312;
uint64_t kSDCardSizeInBytes = 9000000;

std::string GetDCIMDeviceId(const std::string& unique_id) {
  return StorageInfo::MakeDeviceId(
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM,
      kFSUniqueIdPrefix + unique_id);
}

// A test version of StorageMonitorCros that exposes protected methods to tests.
class TestStorageMonitorCros : public StorageMonitorCros {
 public:
  TestStorageMonitorCros() {}

  ~TestStorageMonitorCros() override {}

  void Init() override {
    mojo::PendingRemote<device::mojom::MtpManager> pending_fake_mtp_manager;
    auto* fake_mtp_manager =
        TestMediaTransferProtocolManagerChromeOS::GetFakeMtpManager();
    fake_mtp_manager->AddReceiver(
        pending_fake_mtp_manager.InitWithNewPipeAndPassReceiver());
    SetMediaTransferProtocolManagerForTest(std::move(pending_fake_mtp_manager));

    StorageMonitorCros::Init();
  }

  void OnMountEvent(
      DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const DiskMountManager::MountPointInfo& mount_info) override {
    StorageMonitorCros::OnMountEvent(event, error_code, mount_info);
  }

  void OnBootDeviceDiskEvent(DiskMountManager::DiskEvent event,
                             const chromeos::disks::Disk& disk) override {
    StorageMonitorCros::OnBootDeviceDiskEvent(event, disk);
  }

  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* device_info) const override {
    return StorageMonitorCros::GetStorageInfoForPath(path, device_info);
  }
  void EjectDevice(const std::string& device_id,
                   base::Callback<void(EjectStatus)> callback) override {
    StorageMonitorCros::EjectDevice(device_id, callback);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestStorageMonitorCros);
};

// Wrapper class to test StorageMonitorCros.
class StorageMonitorCrosTest : public testing::Test {
 public:
  StorageMonitorCrosTest();
  ~StorageMonitorCrosTest() override;

  void EjectNotify(StorageMonitor::EjectStatus status);

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void MountDevice(chromeos::MountError error_code,
                   const DiskMountManager::MountPointInfo& mount_info,
                   const std::string& unique_id,
                   const std::string& device_label,
                   const std::string& vendor_name,
                   const std::string& product_name,
                   chromeos::DeviceType device_type,
                   uint64_t device_size_in_bytes);

  void UnmountDevice(chromeos::MountError error_code,
                     const DiskMountManager::MountPointInfo& mount_info);

  uint64_t GetDeviceStorageSize(const std::string& device_location);

  // Create a directory named |dir| relative to the test directory.
  // Set |with_dcim_dir| to true if the created directory will have a "DCIM"
  // subdirectory.
  // Returns the full path to the created directory on success, or an empty
  // path on failure.
  base::FilePath CreateMountPoint(const std::string& dir, bool with_dcim_dir);

  MockRemovableStorageObserver& observer() {
    return *mock_storage_observer_;
  }

  TestStorageMonitorCros* monitor_;

  // Owned by DiskMountManager.
  chromeos::disks::MockDiskMountManager* disk_mount_manager_mock_;

  StorageMonitor::EjectStatus status_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  // Temporary directory for created test data.
  base::ScopedTempDir scoped_temp_dir_;

  // Objects that talks with StorageMonitorCros.
  std::unique_ptr<MockRemovableStorageObserver> mock_storage_observer_;

  DISALLOW_COPY_AND_ASSIGN(StorageMonitorCrosTest);
};

StorageMonitorCrosTest::StorageMonitorCrosTest()
    : monitor_(NULL),
      disk_mount_manager_mock_(NULL),
      status_(StorageMonitor::EJECT_FAILURE) {}

StorageMonitorCrosTest::~StorageMonitorCrosTest() {
}

void StorageMonitorCrosTest::SetUp() {
  ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager();
  DiskMountManager::InitializeForTesting(disk_mount_manager_mock_);
  disk_mount_manager_mock_->SetupDefaultReplies();

  mock_storage_observer_.reset(new MockRemovableStorageObserver);

  // Initialize the test subject.
  TestStorageMonitor::Destroy();
  monitor_ = new TestStorageMonitorCros();
  std::unique_ptr<StorageMonitor> pass_monitor(monitor_);
  StorageMonitor::SetStorageMonitorForTesting(std::move(pass_monitor));

  monitor_->Init();
  monitor_->AddObserver(mock_storage_observer_.get());
}

void StorageMonitorCrosTest::TearDown() {
  monitor_->RemoveObserver(mock_storage_observer_.get());
  monitor_ = NULL;

  disk_mount_manager_mock_ = NULL;
  DiskMountManager::Shutdown();
  task_environment_.RunUntilIdle();
}

void StorageMonitorCrosTest::MountDevice(
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info,
    const std::string& unique_id,
    const std::string& device_label,
    const std::string& vendor_name,
    const std::string& product_name,
    chromeos::DeviceType device_type,
    uint64_t device_size_in_bytes) {
  if (error_code == chromeos::MOUNT_ERROR_NONE) {
    disk_mount_manager_mock_->CreateDiskEntryForMountDevice(
        mount_info, unique_id, device_label, vendor_name, product_name,
        device_type, device_size_in_bytes, false /* is_parent */,
        true /* has_media */, false /* on_boot_device */,
        true /* on_removable_device */, kFileSystemType);
  }
  monitor_->OnMountEvent(DiskMountManager::MOUNTING, error_code, mount_info);
  task_environment_.RunUntilIdle();
}

void StorageMonitorCrosTest::UnmountDevice(
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  monitor_->OnMountEvent(DiskMountManager::UNMOUNTING, error_code, mount_info);
  if (error_code == chromeos::MOUNT_ERROR_NONE)
    disk_mount_manager_mock_->RemoveDiskEntryForMountDevice(mount_info);
  task_environment_.RunUntilIdle();
}

uint64_t StorageMonitorCrosTest::GetDeviceStorageSize(
    const std::string& device_location) {
  StorageInfo info;
  if (!monitor_->GetStorageInfoForPath(base::FilePath(device_location), &info))
    return 0;

  return info.total_size_in_bytes();
}

base::FilePath StorageMonitorCrosTest::CreateMountPoint(
    const std::string& dir, bool with_dcim_dir) {
  base::FilePath return_path(scoped_temp_dir_.GetPath());
  return_path = return_path.AppendASCII(dir);
  base::FilePath path(return_path);
  if (with_dcim_dir)
    path = path.Append(kDCIMDirectoryName);
  if (!base::CreateDirectory(path))
    return base::FilePath();
  return return_path;
}

void StorageMonitorCrosTest::EjectNotify(StorageMonitor::EjectStatus status) {
  status_ = status;
}

// Simple test case where we attach and detach a media device.
TEST_F(StorageMonitorCrosTest, BasicAttachDetach) {
  base::FilePath mount_path1 = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info(
      kDevice1,
      mount_path1.value(),
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info,
              kUniqueId1,
              kDevice1Name,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_USB,
              kDevice1SizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1),
            observer().last_attached().device_id());
  EXPECT_EQ(mount_path1.value(), observer().last_attached().location());

  UnmountDevice(chromeos::MOUNT_ERROR_NONE, mount_info);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1),
            observer().last_detached().device_id());

  base::FilePath mount_path2 = CreateMountPoint(kMountPointB, true);
  ASSERT_FALSE(mount_path2.empty());
  DiskMountManager::MountPointInfo mount_info2(
      kDevice2,
      mount_path2.value(),
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info2,
              kUniqueId2,
              kDevice2Name,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_USB,
              kDevice2SizeInBytes);
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2),
            observer().last_attached().device_id());
  EXPECT_EQ(mount_path2.value(), observer().last_attached().location());

  UnmountDevice(chromeos::MOUNT_ERROR_NONE, mount_info2);
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2),
            observer().last_detached().device_id());
}

// Removable mass storage devices with no dcim folder are also recognized.
TEST_F(StorageMonitorCrosTest, NoDCIM) {
  testing::Sequence mock_sequence;
  base::FilePath mount_path = CreateMountPoint(kMountPointA, false);
  const std::string kUniqueId = "FFFF-FFFF";
  ASSERT_FALSE(mount_path.empty());
  DiskMountManager::MountPointInfo mount_info(
      kDevice1,
      mount_path.value(),
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  const std::string device_id = StorageInfo::MakeDeviceId(
      StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM,
      kFSUniqueIdPrefix + kUniqueId);
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info,
              kUniqueId,
              kDevice1Name,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_USB,
              kDevice1SizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(device_id, observer().last_attached().device_id());
  EXPECT_EQ(mount_path.value(), observer().last_attached().location());
}

// Non device mounts and mount errors are ignored.
TEST_F(StorageMonitorCrosTest, Ignore) {
  testing::Sequence mock_sequence;
  base::FilePath mount_path = CreateMountPoint(kMountPointA, true);
  const std::string kUniqueId = "FFFF-FFFF";
  ASSERT_FALSE(mount_path.empty());

  // Mount error.
  DiskMountManager::MountPointInfo mount_info(
      kDevice1,
      mount_path.value(),
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountDevice(chromeos::MOUNT_ERROR_UNKNOWN,
              mount_info,
              kUniqueId,
              kDevice1Name,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_USB,
              kDevice1SizeInBytes);
  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Not a device
  mount_info.mount_type = chromeos::MOUNT_TYPE_ARCHIVE;
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info,
              kUniqueId,
              kDevice1Name,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_USB,
              kDevice1SizeInBytes);
  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Unsupported file system.
  mount_info.mount_type = chromeos::MOUNT_TYPE_DEVICE;
  mount_info.mount_condition =
      chromeos::disks::MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM;
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info,
              kUniqueId,
              kDevice1Name,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_USB,
              kDevice1SizeInBytes);
  EXPECT_EQ(0, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
}

TEST_F(StorageMonitorCrosTest, SDCardAttachDetach) {
  base::FilePath mount_path1 = CreateMountPoint(kSDCardMountPoint1, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info1(
      kSDCardDeviceName1,
      mount_path1.value(),
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info1,
              kUniqueId2,
              kSDCardDeviceName1,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_SD,
              kSDCardSizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2),
            observer().last_attached().device_id());
  EXPECT_EQ(mount_path1.value(), observer().last_attached().location());

  UnmountDevice(chromeos::MOUNT_ERROR_NONE, mount_info1);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2),
            observer().last_detached().device_id());

  base::FilePath mount_path2 = CreateMountPoint(kSDCardMountPoint2, true);
  ASSERT_FALSE(mount_path2.empty());
  DiskMountManager::MountPointInfo mount_info2(
      kSDCardDeviceName2,
      mount_path2.value(),
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info2,
              kUniqueId2,
              kSDCardDeviceName2,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_SD,
              kSDCardSizeInBytes);
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2),
            observer().last_attached().device_id());
  EXPECT_EQ(mount_path2.value(), observer().last_attached().location());

  UnmountDevice(chromeos::MOUNT_ERROR_NONE, mount_info2);
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId2),
            observer().last_detached().device_id());
}

TEST_F(StorageMonitorCrosTest, AttachDeviceWithEmptyLabel) {
  base::FilePath mount_path1 = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info(
      kEmptyDeviceLabel,
      mount_path1.value(),
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info,
              kUniqueId1,
              kEmptyDeviceLabel,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_USB,
              kDevice1SizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1),
            observer().last_attached().device_id());
  EXPECT_EQ(mount_path1.value(), observer().last_attached().location());

  UnmountDevice(chromeos::MOUNT_ERROR_NONE, mount_info);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1),
            observer().last_detached().device_id());
}

TEST_F(StorageMonitorCrosTest, GetStorageSize) {
  base::FilePath mount_path1 = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info(
      kEmptyDeviceLabel,
      mount_path1.value(),
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info,
              kUniqueId1,
              kEmptyDeviceLabel,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_USB,
              kDevice1SizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1),
            observer().last_attached().device_id());
  EXPECT_EQ(mount_path1.value(), observer().last_attached().location());

  EXPECT_EQ(kDevice1SizeInBytes, GetDeviceStorageSize(mount_path1.value()));
  UnmountDevice(chromeos::MOUNT_ERROR_NONE, mount_info);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDCIMDeviceId(kUniqueId1),
            observer().last_detached().device_id());
}

TEST_F(StorageMonitorCrosTest, EjectTest) {
  base::FilePath mount_path1 = CreateMountPoint(kMountPointA, true);
  ASSERT_FALSE(mount_path1.empty());
  DiskMountManager::MountPointInfo mount_info(
      kEmptyDeviceLabel,
      mount_path1.value(),
      chromeos::MOUNT_TYPE_DEVICE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountDevice(chromeos::MOUNT_ERROR_NONE,
              mount_info,
              kUniqueId1,
              kEmptyDeviceLabel,
              kVendorName,
              kProductName,
              chromeos::DEVICE_TYPE_USB,
              kDevice1SizeInBytes);
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // testing::Invoke doesn't handle move-only types, so use a lambda instead.
  ON_CALL(*disk_mount_manager_mock_, UnmountPath(_, _))
      .WillByDefault([](const std::string& location,
                        DiskMountManager::UnmountPathCallback cb) {
        std::move(cb).Run(chromeos::MOUNT_ERROR_NONE);
      });
  EXPECT_CALL(*disk_mount_manager_mock_,
              UnmountPath(observer().last_attached().location(), _));
  monitor_->EjectDevice(observer().last_attached().device_id(),
                        base::Bind(&StorageMonitorCrosTest::EjectNotify,
                                   base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(StorageMonitor::EJECT_OK, status_);
}

TEST_F(StorageMonitorCrosTest, FixedStroageTest) {
  const std::string uuid = "fixed1-uuid";
  const std::string mount_point = "/mnt/stateful_partition";

  // Fixed storage (stateful partition) added.
  const std::string label = "fixed1";

  std::unique_ptr<const Disk> disk = Disk::Builder()
                                         .SetMountPath(mount_point)
                                         .SetDeviceLabel(label)
                                         .SetFileSystemUUID(uuid)
                                         .Build();
  monitor_->OnBootDeviceDiskEvent(DiskMountManager::DiskEvent::DISK_ADDED,
                                  *disk);
  std::vector<StorageInfo> disks = monitor_->GetAllAvailableStorages();
  ASSERT_EQ(1U, disks.size());
  EXPECT_EQ(mount_point, disks[0].location());
  EXPECT_EQ(base::ASCIIToUTF16(label), disks[0].storage_label());

  // Fixed storage (not stateful partition) added - ignore.
  std::unique_ptr<const Disk> ignored_disk =
      Disk::Builder()
          .SetMountPath("usr/share/OEM")
          .SetDeviceLabel("fixed2")
          .SetFileSystemUUID("fixed2-uuid")
          .Build();
  monitor_->OnBootDeviceDiskEvent(DiskMountManager::DiskEvent::DISK_ADDED,
                                  *ignored_disk);
  disks = monitor_->GetAllAvailableStorages();
  ASSERT_EQ(1U, disks.size());
  EXPECT_EQ(mount_point, disks[0].location());
  EXPECT_EQ(base::ASCIIToUTF16(label), disks[0].storage_label());

  // Fixed storage (stateful partition) removed.
  monitor_->OnBootDeviceDiskEvent(DiskMountManager::DiskEvent::DISK_REMOVED,
                                  *disk);
  disks = monitor_->GetAllAvailableStorages();
  EXPECT_EQ(0U, disks.size());
}

}  // namespace

}  // namespace storage_monitor

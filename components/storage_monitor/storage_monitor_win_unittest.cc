// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_monitor_win.h"

#include <windows.h>

#include <dbt.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "components/storage_monitor/mock_removable_storage_observer.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor_win.h"
#include "components/storage_monitor/test_volume_mount_watcher_win.h"
#include "components/storage_monitor/volume_mount_watcher_win.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

typedef std::vector<int> DeviceIndices;

// StorageMonitorWinTest -------------------------------------------------------

namespace storage_monitor {

class StorageMonitorWinTest : public testing::Test {
 public:
  StorageMonitorWinTest();

  StorageMonitorWinTest(const StorageMonitorWinTest&) = delete;
  StorageMonitorWinTest& operator=(const StorageMonitorWinTest&) = delete;

  ~StorageMonitorWinTest() override;

 protected:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void PreAttachDevices();

  void DoMassStorageDeviceAttachedTest(const DeviceIndices& device_indices);
  void DoMassStorageDevicesDetachedTest(const DeviceIndices& device_indices);

  std::unique_ptr<TestStorageMonitorWin> monitor_;

  // Weak pointer; owned by the device notifications class.
  raw_ptr<TestVolumeMountWatcherWin> volume_mount_watcher_;

  MockRemovableStorageObserver observer_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

StorageMonitorWinTest::StorageMonitorWinTest() {
}

StorageMonitorWinTest::~StorageMonitorWinTest() {
}

void StorageMonitorWinTest::SetUp() {
  auto volume_mount_watcher = std::make_unique<TestVolumeMountWatcherWin>();
  volume_mount_watcher_ = volume_mount_watcher.get();
  monitor_ =
      std::make_unique<TestStorageMonitorWin>(std::move(volume_mount_watcher));

  monitor_->Init();
  content::RunAllTasksUntilIdle();
  monitor_->AddObserver(&observer_);
}

void StorageMonitorWinTest::TearDown() {
  content::RunAllTasksUntilIdle();
  monitor_->RemoveObserver(&observer_);

  // Windows storage monitor must be destroyed on the same thread
  // as construction.
  volume_mount_watcher_ = nullptr;
  monitor_.reset();
}

void StorageMonitorWinTest::PreAttachDevices() {
  volume_mount_watcher_ = nullptr;
  monitor_.reset();
  auto volume_mount_watcher = std::make_unique<TestVolumeMountWatcherWin>();
  volume_mount_watcher_ = volume_mount_watcher.get();
  volume_mount_watcher_->SetAttachedDevicesFake();

  int expect_attach_calls = 0;
  std::vector<base::FilePath> initial_devices =
      volume_mount_watcher_->GetAttachedDevicesCallback().Run();
  for (std::vector<base::FilePath>::const_iterator it = initial_devices.begin();
       it != initial_devices.end(); ++it) {
    bool removable;
    ASSERT_TRUE(volume_mount_watcher_->GetDeviceRemovable(*it, &removable));
    if (removable)
      expect_attach_calls++;
  }

  monitor_ =
      std::make_unique<TestStorageMonitorWin>(std::move(volume_mount_watcher));

  monitor_->AddObserver(&observer_);
  monitor_->Init();

  EXPECT_EQ(0u, volume_mount_watcher_->devices_checked().size());

  content::RunAllTasksUntilIdle();

  std::vector<base::FilePath> checked_devices =
      volume_mount_watcher_->devices_checked();
  sort(checked_devices.begin(), checked_devices.end());
  EXPECT_EQ(initial_devices, checked_devices);
  EXPECT_EQ(expect_attach_calls, observer_.attach_calls());
  EXPECT_EQ(0, observer_.detach_calls());
}

void StorageMonitorWinTest::DoMassStorageDeviceAttachedTest(
    const DeviceIndices& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;

  int expect_attach_calls = observer_.attach_calls();
  for (DeviceIndices::const_iterator it = device_indices.begin();
       it != device_indices.end(); ++it) {
    volume_broadcast.dbcv_unitmask |= 0x1 << *it;
    bool removable;
    ASSERT_TRUE(volume_mount_watcher_->GetDeviceRemovable(
        VolumeMountWatcherWin::DriveNumberToFilePath(*it), &removable));
    if (removable)
      expect_attach_calls++;
  }
  monitor_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                               reinterpret_cast<LPARAM>(&volume_broadcast));

  content::RunAllTasksUntilIdle();

  EXPECT_EQ(expect_attach_calls, observer_.attach_calls());
  EXPECT_EQ(0, observer_.detach_calls());
}

void StorageMonitorWinTest::DoMassStorageDevicesDetachedTest(
    const DeviceIndices& device_indices) {
  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_unitmask = 0x0;
  volume_broadcast.dbcv_flags = 0x0;

  int pre_attach_calls = observer_.attach_calls();
  int expect_detach_calls = 0;
  for (DeviceIndices::const_iterator it = device_indices.begin();
       it != device_indices.end(); ++it) {
    volume_broadcast.dbcv_unitmask |= 0x1 << *it;
    StorageInfo info;
    ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(
        VolumeMountWatcherWin::DriveNumberToFilePath(*it), &info));
    if (StorageInfo::IsRemovableDevice(info.device_id()))
      ++expect_detach_calls;
  }
  monitor_->InjectDeviceChange(DBT_DEVICEREMOVECOMPLETE,
                               reinterpret_cast<LPARAM>(&volume_broadcast));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(pre_attach_calls, observer_.attach_calls());
  EXPECT_EQ(expect_detach_calls, observer_.detach_calls());
}

TEST_F(StorageMonitorWinTest, RandomMessage) {
  monitor_->InjectDeviceChange(DBT_DEVICEQUERYREMOVE, NULL);
  content::RunAllTasksUntilIdle();
}

TEST_F(StorageMonitorWinTest, DevicesAttached) {
  DeviceIndices device_indices;
  device_indices.push_back(1);  // B
  device_indices.push_back(5);  // F
  device_indices.push_back(7);  // H
  device_indices.push_back(13);  // N
  DoMassStorageDeviceAttachedTest(device_indices);

  StorageInfo info;
  EXPECT_TRUE(monitor_->volume_mount_watcher()->GetDeviceInfo(
      base::FilePath(FILE_PATH_LITERAL("F:\\")), &info));
  EXPECT_EQ(L"F:\\", info.location());
  EXPECT_EQ("dcim:\\\\?\\Volume{F0000000-0000-0000-0000-000000000000}\\",
            info.device_id());
  EXPECT_EQ(u"F:\\ Drive", info.storage_label());

  EXPECT_FALSE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("G:\\")), &info));
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\")), &info));
  StorageInfo info1;
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\subdir")), &info1));
  StorageInfo info2;
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\subdir\\sub")), &info2));
  EXPECT_EQ(u"F:\\ Drive", info.storage_label());
  EXPECT_EQ(u"F:\\ Drive", info1.storage_label());
  EXPECT_EQ(u"F:\\ Drive", info2.storage_label());
}

TEST_F(StorageMonitorWinTest, PathMountDevices) {
  PreAttachDevices();
  size_t init_storages = monitor_->GetAllAvailableStorages().size();

  volume_mount_watcher_->AddDeviceForTesting(
      base::FilePath(FILE_PATH_LITERAL("F:\\mount1")), "dcim:mount1", u"mount1",
      100);
  volume_mount_watcher_->AddDeviceForTesting(
      base::FilePath(FILE_PATH_LITERAL("F:\\mount1\\subdir")),
      "dcim:mount1subdir", u"mount1subdir", 100);
  volume_mount_watcher_->AddDeviceForTesting(
      base::FilePath(FILE_PATH_LITERAL("F:\\mount2")), "dcim:mount2", u"mount2",
      100);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(init_storages + 3, monitor_->GetAllAvailableStorages().size());

  StorageInfo info;
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\dir")), &info));
  EXPECT_EQ(u"F:\\ Drive", info.GetDisplayName(false));
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\mount1")), &info));
  EXPECT_EQ(u"mount1", info.GetDisplayName(false));
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\mount1\\dir")), &info));
  EXPECT_EQ(u"mount1", info.GetDisplayName(false));
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\mount2\\dir")), &info));
  EXPECT_EQ(u"mount2", info.GetDisplayName(false));
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\mount1\\subdir")), &info));
  EXPECT_EQ(u"mount1subdir", info.GetDisplayName(false));
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\mount1\\subdir\\dir")), &info));
  EXPECT_EQ(u"mount1subdir", info.GetDisplayName(false));
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(
      base::FilePath(FILE_PATH_LITERAL("F:\\mount1\\subdir\\dir\\dir")),
      &info));
  EXPECT_EQ(u"mount1subdir", info.GetDisplayName(false));
}

TEST_F(StorageMonitorWinTest, DevicesAttachedHighBoundary) {
  DeviceIndices device_indices;
  device_indices.push_back(25);

  DoMassStorageDeviceAttachedTest(device_indices);
}

TEST_F(StorageMonitorWinTest, DevicesAttachedLowBoundary) {
  DeviceIndices device_indices;
  device_indices.push_back(0);

  DoMassStorageDeviceAttachedTest(device_indices);
}

TEST_F(StorageMonitorWinTest, DevicesAttachedAdjacentBits) {
  DeviceIndices device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoMassStorageDeviceAttachedTest(device_indices);
}

TEST_F(StorageMonitorWinTest, DevicesDetached) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(1);
  device_indices.push_back(5);
  device_indices.push_back(7);
  device_indices.push_back(13);

  DoMassStorageDevicesDetachedTest(device_indices);
}

TEST_F(StorageMonitorWinTest, DevicesDetachedHighBoundary) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(25);

  DoMassStorageDevicesDetachedTest(device_indices);
}

TEST_F(StorageMonitorWinTest, DevicesDetachedLowBoundary) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(0);

  DoMassStorageDevicesDetachedTest(device_indices);
}

TEST_F(StorageMonitorWinTest, DevicesDetachedAdjacentBits) {
  PreAttachDevices();

  DeviceIndices device_indices;
  device_indices.push_back(0);
  device_indices.push_back(1);
  device_indices.push_back(2);
  device_indices.push_back(3);

  DoMassStorageDevicesDetachedTest(device_indices);
}

TEST_F(StorageMonitorWinTest, DuplicateAttachCheckSuppressed) {
  // Make sure the original C: mount notification makes it all the
  // way through.
  content::RunAllTasksUntilIdle();

  volume_mount_watcher_->BlockDeviceCheckForTesting();
  base::FilePath kAttachedDevicePath =
      VolumeMountWatcherWin::DriveNumberToFilePath(8);  // I:

  DEV_BROADCAST_VOLUME volume_broadcast;
  volume_broadcast.dbcv_size = sizeof(volume_broadcast);
  volume_broadcast.dbcv_devicetype = DBT_DEVTYP_VOLUME;
  volume_broadcast.dbcv_flags = 0x0;
  volume_broadcast.dbcv_unitmask = 0x100;  // I: drive
  monitor_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                               reinterpret_cast<LPARAM>(&volume_broadcast));

  EXPECT_EQ(0u, volume_mount_watcher_->devices_checked().size());

  // Re-attach the same volume. We haven't released the mock device check
  // event, so there'll be pending calls in the UI thread to finish the
  // device check notification, blocking the duplicate device injection.
  monitor_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                               reinterpret_cast<LPARAM>(&volume_broadcast));

  EXPECT_EQ(0u, volume_mount_watcher_->devices_checked().size());
  volume_mount_watcher_->ReleaseDeviceCheck();
  content::RunAllTasksUntilIdle();
  volume_mount_watcher_->ReleaseDeviceCheck();

  // Now let all attach notifications finish running. We'll only get one
  // finish-attach call.
  content::RunAllTasksUntilIdle();

  const std::vector<base::FilePath>& checked_devices =
      volume_mount_watcher_->devices_checked();
  ASSERT_EQ(1u, checked_devices.size());
  EXPECT_EQ(kAttachedDevicePath, checked_devices[0]);

  // We'll receive a duplicate check now that the first check has fully cleared.
  monitor_->InjectDeviceChange(DBT_DEVICEARRIVAL,
                               reinterpret_cast<LPARAM>(&volume_broadcast));
  content::RunAllTasksUntilIdle();
  volume_mount_watcher_->ReleaseDeviceCheck();
  content::RunAllTasksUntilIdle();

  ASSERT_EQ(2u, checked_devices.size());
  EXPECT_EQ(kAttachedDevicePath, checked_devices[0]);
  EXPECT_EQ(kAttachedDevicePath, checked_devices[1]);
}

TEST_F(StorageMonitorWinTest, DeviceInfoForPath) {
  PreAttachDevices();

  StorageInfo device_info;
  // An invalid path.
  EXPECT_FALSE(monitor_->GetStorageInfoForPath(base::FilePath(L"COM1:\\"),
                                               &device_info));

  // An unconnected removable device.
  EXPECT_FALSE(monitor_->GetStorageInfoForPath(base::FilePath(L"E:\\"),
                                               &device_info));

  // A connected removable device.
  base::FilePath removable_device(L"F:\\");
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(removable_device, &device_info));

  StorageInfo info;
  ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(removable_device, &info));
  EXPECT_TRUE(StorageInfo::IsRemovableDevice(info.device_id()));
  EXPECT_EQ(info.device_id(), device_info.device_id());
  EXPECT_EQ(info.GetDisplayName(false), device_info.GetDisplayName(false));
  EXPECT_EQ(info.location(), device_info.location());
  EXPECT_EQ(1000000u, info.total_size_in_bytes());

  // A fixed device.
  base::FilePath fixed_device(L"N:\\");
  EXPECT_TRUE(monitor_->GetStorageInfoForPath(fixed_device, &device_info));

  ASSERT_TRUE(volume_mount_watcher_->GetDeviceInfo(
      fixed_device, &info));
  EXPECT_FALSE(StorageInfo::IsRemovableDevice(info.device_id()));
  EXPECT_EQ(info.device_id(), device_info.device_id());
  EXPECT_EQ(info.GetDisplayName(false), device_info.GetDisplayName(false));
  EXPECT_EQ(info.location(), device_info.location());
}

TEST_F(StorageMonitorWinTest, DriveNumberToFilePath) {
  EXPECT_EQ(L"A:\\", VolumeMountWatcherWin::DriveNumberToFilePath(0).value());
  EXPECT_EQ(L"Y:\\", VolumeMountWatcherWin::DriveNumberToFilePath(24).value());
  EXPECT_EQ(L"", VolumeMountWatcherWin::DriveNumberToFilePath(-1).value());
  EXPECT_EQ(L"", VolumeMountWatcherWin::DriveNumberToFilePath(199).value());
}

}  // namespace storage_monitor

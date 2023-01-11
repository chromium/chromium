// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/storage_monitor/storage_monitor.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "components/storage_monitor/mock_removable_storage_observer.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void SetLatch(bool* called) {
  *called = true;
}

}  // namespace

namespace storage_monitor {

TEST(StorageMonitorTest, TestInitialize) {
  base::test::SingleThreadTaskEnvironment task_environment;
  TestStorageMonitor::Destroy();
  TestStorageMonitor monitor;
  EXPECT_FALSE(monitor.init_called());

  bool initialized = false;
  monitor.EnsureInitialized(base::BindOnce(&SetLatch, &initialized));
  EXPECT_TRUE(monitor.init_called());
  EXPECT_FALSE(initialized);
  monitor.MarkInitialized();
  EXPECT_TRUE(initialized);
}

TEST(StorageMonitorTest, DeviceAttachDetachNotifications) {
  TestStorageMonitor::Destroy();
  base::test::SingleThreadTaskEnvironment task_environment;
  const std::string kDeviceId1 = "dcim:UUID:FFF0-0001";
  const std::string kDeviceId2 = "dcim:UUID:FFF0-0002";
  MockRemovableStorageObserver observer1;
  MockRemovableStorageObserver observer2;
  TestStorageMonitor monitor;
  monitor.AddObserver(&observer1);
  monitor.AddObserver(&observer2);

  StorageInfo info(kDeviceId1, FILE_PATH_LITERAL("path"), std::u16string(),
                   std::u16string(), std::u16string(), 0);
  monitor.receiver()->ProcessAttach(info);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kDeviceId1, observer1.last_attached().device_id());
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer1.last_attached().location());
  EXPECT_EQ(kDeviceId1, observer2.last_attached().device_id());
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer2.last_attached().location());
  EXPECT_EQ(1, observer1.attach_calls());
  EXPECT_EQ(0, observer1.detach_calls());

  monitor.receiver()->ProcessDetach(kDeviceId1);
  monitor.receiver()->ProcessDetach(kDeviceId2);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kDeviceId1, observer1.last_detached().device_id());
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer1.last_detached().location());
  EXPECT_EQ(kDeviceId1, observer2.last_detached().device_id());
  EXPECT_EQ(FILE_PATH_LITERAL("path"), observer2.last_detached().location());

  EXPECT_EQ(1, observer1.attach_calls());
  EXPECT_EQ(1, observer2.attach_calls());

  // The kDeviceId2 won't be notified since it was never attached.
  EXPECT_EQ(1, observer1.detach_calls());
  EXPECT_EQ(1, observer2.detach_calls());

  monitor.RemoveObserver(&observer1);
  monitor.RemoveObserver(&observer2);
}

TEST(StorageMonitorTest, GetAllAvailableStoragesEmpty) {
  TestStorageMonitor::Destroy();
  base::test::SingleThreadTaskEnvironment task_environment;
  TestStorageMonitor monitor;
  std::vector<StorageInfo> devices = monitor.GetAllAvailableStorages();
  EXPECT_EQ(0U, devices.size());
}

TEST(StorageMonitorTest, GetAllAvailableStorageAttachDetach) {
  TestStorageMonitor::Destroy();
  base::test::SingleThreadTaskEnvironment task_environment;
  TestStorageMonitor monitor;
  const std::string kDeviceId1 = "dcim:UUID:FFF0-0042";
  const base::FilePath kDevicePath1(FILE_PATH_LITERAL("/testfoo"));
  StorageInfo info1(kDeviceId1, kDevicePath1.value(), std::u16string(),
                    std::u16string(), std::u16string(), 0);
  monitor.receiver()->ProcessAttach(info1);
  base::RunLoop().RunUntilIdle();
  std::vector<StorageInfo> devices = monitor.GetAllAvailableStorages();
  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(kDeviceId1, devices[0].device_id());
  EXPECT_EQ(kDevicePath1.value(), devices[0].location());

  const std::string kDeviceId2 = "dcim:UUID:FFF0-0044";
  const base::FilePath kDevicePath2(FILE_PATH_LITERAL("/testbar"));
  StorageInfo info2(kDeviceId2, kDevicePath2.value(), std::u16string(),
                    std::u16string(), std::u16string(), 0);
  monitor.receiver()->ProcessAttach(info2);
  base::RunLoop().RunUntilIdle();
  devices = monitor.GetAllAvailableStorages();
  ASSERT_EQ(2U, devices.size());
  EXPECT_EQ(kDeviceId1, devices[0].device_id());
  EXPECT_EQ(kDevicePath1.value(), devices[0].location());
  EXPECT_EQ(kDeviceId2, devices[1].device_id());
  EXPECT_EQ(kDevicePath2.value(), devices[1].location());

  monitor.receiver()->ProcessDetach(kDeviceId1);
  base::RunLoop().RunUntilIdle();
  devices = monitor.GetAllAvailableStorages();
  ASSERT_EQ(1U, devices.size());
  EXPECT_EQ(kDeviceId2, devices[0].device_id());
  EXPECT_EQ(kDevicePath2.value(), devices[0].location());

  monitor.receiver()->ProcessDetach(kDeviceId2);
  base::RunLoop().RunUntilIdle();
  devices = monitor.GetAllAvailableStorages();
  EXPECT_EQ(0U, devices.size());
}

}  // namespace storage_monitor

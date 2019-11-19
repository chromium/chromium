// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StorageMonitorLinux unit tests.

#include "components/storage_monitor/storage_monitor_linux.h"

#include <mntent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/storage_monitor/mock_removable_storage_observer.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage_monitor {

namespace {

const char kValidFS[] = "vfat";
const char kInvalidFS[] = "invalidfs";

const char kInvalidPath[] = "invalid path does not exist";

const char kDeviceDCIM1[] = "d1";
const char kDeviceDCIM2[] = "d2";
const char kDeviceDCIM3[] = "d3";
const char kDeviceNoDCIM[] = "d4";
const char kDeviceFixed[] = "d5";

const char kInvalidDevice[] = "invalid_device";

const char kMountPointA[] = "mnt_a";
const char kMountPointB[] = "mnt_b";
const char kMountPointC[] = "mnt_c";

struct TestDeviceData {
  const char* device_path;
  const char* unique_id;
  StorageInfo::Type type;
  uint64_t partition_size_in_bytes;
};

const TestDeviceData kTestDeviceData[] = {
  { kDeviceDCIM1, "UUID:FFF0-000F",
    StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM, 88788 },
  { kDeviceDCIM2, "VendorModelSerial:ComName:Model2010:8989",
    StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM,
    8773 },
  { kDeviceDCIM3, "VendorModelSerial:::WEM319X792",
    StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM, 22837 },
  { kDeviceNoDCIM, "UUID:ABCD-1234",
    StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM, 512 },
  { kDeviceFixed, "UUID:743A-2349",
    StorageInfo::FIXED_MASS_STORAGE, 17282 },
};

std::unique_ptr<StorageInfo> GetDeviceInfo(const base::FilePath& device_path,
                                           const base::FilePath& mount_point) {
  bool device_found = false;
  size_t i = 0;
  for (; i < base::size(kTestDeviceData); i++) {
    if (device_path.value() == kTestDeviceData[i].device_path) {
      device_found = true;
      break;
    }
  }
  DCHECK(device_found);

  StorageInfo::Type type = kTestDeviceData[i].type;
  auto storage_info = std::make_unique<StorageInfo>(
      StorageInfo::MakeDeviceId(type, kTestDeviceData[i].unique_id),
      mount_point.value(), base::ASCIIToUTF16("volume label"),
      base::ASCIIToUTF16("vendor name"), base::ASCIIToUTF16("model name"),
      kTestDeviceData[i].partition_size_in_bytes);
  return storage_info;
}

uint64_t GetDevicePartitionSize(const std::string& device) {
  for (const auto& data : kTestDeviceData) {
    if (device == data.device_path)
      return data.partition_size_in_bytes;
  }
  return 0;
}

std::string GetDeviceId(const std::string& device) {
  for (const auto& data : kTestDeviceData) {
    if (device == data.device_path)
      return StorageInfo::MakeDeviceId(data.type, data.unique_id);
  }
  if (device == kInvalidDevice) {
    return StorageInfo::MakeDeviceId(StorageInfo::FIXED_MASS_STORAGE,
                                     kInvalidDevice);
  }
  return std::string();
}

class TestStorageMonitorLinux : public StorageMonitorLinux {
 public:
  explicit TestStorageMonitorLinux(const base::FilePath& path)
      : StorageMonitorLinux(path) {
    SetGetDeviceInfoCallbackForTest(base::BindRepeating(&GetDeviceInfo));
  }
  ~TestStorageMonitorLinux() override = default;

 private:
  void UpdateMtab(
      const MtabWatcherLinux::MountPointDeviceMap& new_mtab) override {
    StorageMonitorLinux::UpdateMtab(new_mtab);

    // The UpdateMtab call performs the actual mounting by posting tasks
    // to the thread pool. This also needs to be flushed.
    base::ThreadPoolInstance::Get()->FlushForTesting();

    // Once the storage monitor picks up the changes to the fake mtab file,
    // exit the RunLoop that should be blocking the main test thread.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
  }

  DISALLOW_COPY_AND_ASSIGN(TestStorageMonitorLinux);
};

class StorageMonitorLinuxTest : public testing::Test {
 public:
  struct MtabTestData {
    MtabTestData(const std::string& mount_device,
                 const std::string& mount_point,
                 const std::string& mount_type)
        : mount_device(mount_device),
          mount_point(mount_point),
          mount_type(mount_type) {
    }

    const std::string mount_device;
    const std::string mount_point;
    const std::string mount_type;
  };

  StorageMonitorLinuxTest() = default;
  ~StorageMonitorLinuxTest() override = default;

 protected:
  void SetUp() override {
    // Create and set up a temp dir with files for the test.
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    base::FilePath test_dir =
        scoped_temp_dir_.GetPath().AppendASCII("test_etc");
    ASSERT_TRUE(base::CreateDirectory(test_dir));
    mtab_file_ = test_dir.AppendASCII("test_mtab");
    MtabTestData initial_test_data[] = {
      MtabTestData("dummydevice", "dummydir", kInvalidFS),
    };
    WriteToMtab(initial_test_data, base::size(initial_test_data),
                /*overwrite=*/true);

    monitor_ = std::make_unique<TestStorageMonitorLinux>(mtab_file_);
    mock_storage_observer_ = std::make_unique<MockRemovableStorageObserver>();
    monitor_->AddObserver(mock_storage_observer_.get());

    monitor_->Init();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    monitor_->RemoveObserver(mock_storage_observer_.get());
    task_environment_.RunUntilIdle();

    // Linux storage monitor must be destroyed on the UI thread, so do it here.
    monitor_.reset();
  }

  // Append mtab entries from the |data| array of size |data_size| to the mtab
  // file, and run the message loop.
  void AppendToMtabAndRunLoop(const MtabTestData* data, size_t data_size) {
    WriteToMtab(data, data_size, /*overwrite=*/false);
    // Block until the mtab changes are detected by the file watcher.
    base::RunLoop().Run();
  }

  // Overwrite the mtab file with mtab entries from the |data| array of size
  // |data_size|, and run the message loop.
  void OverwriteMtabAndRunLoop(const MtabTestData* data, size_t data_size) {
    WriteToMtab(data, data_size, /*overwrite=*/true);
    // Block until the mtab changes are detected by the file watcher.
    base::RunLoop().Run();
  }

  // Simplied version of OverwriteMtabAndRunLoop() that just deletes all the
  // entries in the mtab file.
  void WriteEmptyMtabAndRunLoop() {
    OverwriteMtabAndRunLoop(/*data=*/nullptr, /*data_size=*/0);
  }

  // Create a directory named |dir| relative to the test directory.
  // It has a DCIM directory, so StorageMonitorLinux recognizes it as a media
  // directory.
  base::FilePath CreateMountPointWithDCIMDir(const std::string& dir) {
    return CreateMountPoint(dir, /*with_dcim_dir=*/true);
  }

  // Create a directory named |dir| relative to the test directory.
  // It does not have a DCIM directory, so StorageMonitorLinux does not
  // recognize it as a media directory.
  base::FilePath CreateMountPointWithoutDCIMDir(const std::string& dir) {
    return CreateMountPoint(dir, /*with_dcim_dir=*/false);
  }

  void RemoveDCIMDirFromMountPoint(const std::string& dir) {
    base::FilePath dcim =
        scoped_temp_dir_.GetPath().AppendASCII(dir).Append(kDCIMDirectoryName);
    base::DeleteFile(dcim, false);
  }

  MockRemovableStorageObserver& observer() {
    return *mock_storage_observer_;
  }

  StorageMonitor* notifier() {
    return monitor_.get();
  }

  uint64_t GetStorageSize(const base::FilePath& path) {
    StorageInfo info;
    if (!notifier()->GetStorageInfoForPath(path, &info))
      return 0;

    return info.total_size_in_bytes();
  }

 private:
  // Create a directory named |dir| relative to the test directory.
  // Set |with_dcim_dir| to true if the created directory will have a "DCIM"
  // subdirectory.
  // Returns the full path to the created directory on success, or an empty
  // path on failure.
  base::FilePath CreateMountPoint(const std::string& dir, bool with_dcim_dir) {
    base::FilePath return_path(scoped_temp_dir_.GetPath());
    return_path = return_path.AppendASCII(dir);
    base::FilePath path(return_path);
    if (with_dcim_dir)
      path = path.Append(kDCIMDirectoryName);
    if (!base::CreateDirectory(path))
      return base::FilePath();
    return return_path;
  }

  // Write the test mtab data to |mtab_file_|.
  // |data| is an array of mtab entries.
  // |data_size| is the array size of |data|.
  // |overwrite| specifies whether to overwrite |mtab_file_|.
  void WriteToMtab(const MtabTestData* data,
                   size_t data_size,
                   bool overwrite) {
    FILE* file = setmntent(mtab_file_.value().c_str(), overwrite ? "w" : "a");
    ASSERT_TRUE(file);

    // Due to the glibc *mntent() interface design, which is out of our
    // control, the mtnent struct has several char* fields, even though
    // addmntent() does not write to them in the calls below. To make the
    // compiler happy while avoiding making additional copies of strings,
    // we just const_cast() the strings' c_str()s.
    // Assuming addmntent() does not write to the char* fields, this is safe.
    // It is unlikely the platforms this test suite runs on will have an
    // addmntent() implementation that does change the char* fields. If that
    // was ever the case, the test suite will start crashing or failing.
    mntent entry;
    static const char kMountOpts[] = "rw";
    entry.mnt_opts = const_cast<char*>(kMountOpts);
    entry.mnt_freq = 0;
    entry.mnt_passno = 0;
    for (size_t i = 0; i < data_size; ++i) {
      entry.mnt_fsname = const_cast<char*>(data[i].mount_device.c_str());
      entry.mnt_dir = const_cast<char*>(data[i].mount_point.c_str());
      entry.mnt_type = const_cast<char*>(data[i].mount_type.c_str());
      ASSERT_EQ(0, addmntent(file, &entry));
    }
    ASSERT_EQ(1, endmntent(file));
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<MockRemovableStorageObserver> mock_storage_observer_;

  // Temporary directory for created test data.
  base::ScopedTempDir scoped_temp_dir_;
  // Path to the test mtab file.
  base::FilePath mtab_file_;

  std::unique_ptr<TestStorageMonitorLinux> monitor_;

  DISALLOW_COPY_AND_ASSIGN(StorageMonitorLinuxTest);
};

// Simple test case where we attach and detach a media device.
TEST_F(StorageMonitorLinuxTest, BasicAttachDetach) {
  base::FilePath test_path = CreateMountPointWithDCIMDir(kMountPointA);
  ASSERT_FALSE(test_path.empty());
  MtabTestData test_data[] = {
    MtabTestData(kDeviceDCIM2, test_path.value(), kValidFS),
    MtabTestData(kDeviceFixed, kInvalidPath, kValidFS),
  };
  // Only |kDeviceDCIM2| should be attached, since |kDeviceFixed| has a bad
  // path.
  AppendToMtabAndRunLoop(test_data, base::size(test_data));

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceDCIM2), observer().last_attached().device_id());
  EXPECT_EQ(test_path.value(), observer().last_attached().location());

  // |kDeviceDCIM2| should be detached here.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceDCIM2), observer().last_detached().device_id());
}

// Only removable devices are recognized.
// This test is flaky, see https://crbug.com/1012211
TEST_F(StorageMonitorLinuxTest, DISABLED_Removable) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  ASSERT_FALSE(test_path_a.empty());
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
  };
  // |kDeviceDCIM1| should be attached as expected.
  AppendToMtabAndRunLoop(test_data1, base::size(test_data1));

  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), observer().last_attached().device_id());
  EXPECT_EQ(test_path_a.value(), observer().last_attached().location());

  // This should do nothing, since |kDeviceFixed| is not removable.
  base::FilePath test_path_b = CreateMountPointWithoutDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_b.empty());
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceFixed, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data2, base::size(test_data2));
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // |kDeviceDCIM1| should be detached as expected.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), observer().last_detached().device_id());

  // |kDeviceNoDCIM| should be attached as expected.
  MtabTestData test_data3[] = {
    MtabTestData(kDeviceNoDCIM, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data3, base::size(test_data3));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceNoDCIM), observer().last_attached().device_id());
  EXPECT_EQ(test_path_b.value(), observer().last_attached().location());

  // |kDeviceNoDCIM| should be detached as expected.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());
  EXPECT_EQ(GetDeviceId(kDeviceNoDCIM), observer().last_detached().device_id());
}

// More complicated test case with multiple devices on multiple mount points.
TEST_F(StorageMonitorLinuxTest, SwapMountPoints) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  // Attach two devices.
  // (*'d mounts are those StorageMonitor knows about.)
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM2 -> kMountPointB *
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceDCIM2, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data1, base::size(test_data1));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Detach two devices from old mount points and attach the devices at new
  // mount points.
  // kDeviceDCIM1 -> kMountPointB *
  // kDeviceDCIM2 -> kMountPointA *
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceDCIM2, test_path_a.value(), kValidFS),
  };
  OverwriteMtabAndRunLoop(test_data2, base::size(test_data2));
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());

  // Detach all devices.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(4, observer().detach_calls());
}

// More complicated test case with multiple devices on multiple mount points.
TEST_F(StorageMonitorLinuxTest, DISABLED_MultiDevicesMultiMountPoints) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  // Attach two devices.
  // (*'d mounts are those StorageMonitor knows about.)
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM2 -> kMountPointB *
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceDCIM2, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data1, base::size(test_data1));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Attach |kDeviceDCIM1| to |kMountPointB|.
  // |kDeviceDCIM2| is inaccessible, so it is detached. |kDeviceDCIM1| has been
  // attached at |kMountPointB|, but is still accessible from |kMountPointA|.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM2 -> kMountPointB
  // kDeviceDCIM1 -> kMountPointB
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data2, base::size(test_data2));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());

  // Detach |kDeviceDCIM1| from |kMountPointA|, causing a detach and attach
  // event.
  // kDeviceDCIM2 -> kMountPointB
  // kDeviceDCIM1 -> kMountPointB *
  MtabTestData test_data3[] = {
    MtabTestData(kDeviceDCIM2, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  OverwriteMtabAndRunLoop(test_data3, base::size(test_data3));
  EXPECT_EQ(3, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());

  // Attach |kDeviceDCIM1| to |kMountPointA|.
  // kDeviceDCIM2 -> kMountPointB
  // kDeviceDCIM1 -> kMountPointB *
  // kDeviceDCIM1 -> kMountPointA
  MtabTestData test_data4[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data4, base::size(test_data4));
  EXPECT_EQ(3, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());

  // Detach |kDeviceDCIM1| from |kMountPointB|.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM2 -> kMountPointB *
  OverwriteMtabAndRunLoop(test_data1, base::size(test_data1));
  EXPECT_EQ(5, observer().attach_calls());
  EXPECT_EQ(3, observer().detach_calls());

  // Detach all devices.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(5, observer().attach_calls());
  EXPECT_EQ(5, observer().detach_calls());
}

TEST_F(StorageMonitorLinuxTest,
       DISABLED_MultipleMountPointsWithNonDCIMDevices) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  // Attach to one first.
  // (*'d mounts are those StorageMonitor knows about.)
  // kDeviceDCIM1 -> kMountPointA *
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data1, base::size(test_data1));
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Attach |kDeviceDCIM1| to |kMountPointB|.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM1 -> kMountPointB
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data2, base::size(test_data2));
  EXPECT_EQ(1, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  // Attach |kDeviceFixed| (a non-removable device) to |kMountPointA|.
  // kDeviceDCIM1 -> kMountPointA
  // kDeviceDCIM1 -> kMountPointB *
  // kDeviceFixed -> kMountPointA
  MtabTestData test_data3[] = {
    MtabTestData(kDeviceFixed, test_path_a.value(), kValidFS),
  };
  RemoveDCIMDirFromMountPoint(kMountPointA);
  AppendToMtabAndRunLoop(test_data3, base::size(test_data3));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());

  // Detach |kDeviceFixed|.
  // kDeviceDCIM1 -> kMountPointA
  // kDeviceDCIM1 -> kMountPointB *
  MtabTestData test_data4[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  CreateMountPointWithDCIMDir(kMountPointA);
  OverwriteMtabAndRunLoop(test_data4, base::size(test_data4));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());

  // Attach |kDeviceNoDCIM| (a non-DCIM device) to |kMountPointB|.
  // kDeviceDCIM1  -> kMountPointA *
  // kDeviceDCIM1  -> kMountPointB
  // kDeviceNoDCIM -> kMountPointB *
  MtabTestData test_data5[] = {
    MtabTestData(kDeviceNoDCIM, test_path_b.value(), kValidFS),
  };
  base::DeleteFile(test_path_b.Append(kDCIMDirectoryName), false);
  AppendToMtabAndRunLoop(test_data5, base::size(test_data5));
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(2, observer().detach_calls());

  // Detach |kDeviceNoDCIM|.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceDCIM1 -> kMountPointB
  MtabTestData test_data6[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceDCIM1, test_path_b.value(), kValidFS),
  };
  CreateMountPointWithDCIMDir(kMountPointB);
  OverwriteMtabAndRunLoop(test_data6, base::size(test_data6));
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(3, observer().detach_calls());

  // Detach |kDeviceDCIM1| from |kMountPointB|.
  // kDeviceDCIM1 -> kMountPointA *
  OverwriteMtabAndRunLoop(test_data1, base::size(test_data1));
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(3, observer().detach_calls());

  // Detach all devices.
  WriteEmptyMtabAndRunLoop();
  EXPECT_EQ(4, observer().attach_calls());
  EXPECT_EQ(4, observer().detach_calls());
}

TEST_F(StorageMonitorLinuxTest, DeviceLookUp) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithoutDCIMDir(kMountPointB);
  base::FilePath test_path_c = CreateMountPointWithoutDCIMDir(kMountPointC);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());
  ASSERT_FALSE(test_path_c.empty());

  // Attach to one first.
  // (starred mounts are those StorageMonitor knows about.)
  // kDeviceDCIM1  -> kMountPointA *
  // kDeviceNoDCIM -> kMountPointB *
  // kDeviceFixed  -> kMountPointC
  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceNoDCIM, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceFixed, test_path_c.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data1, base::size(test_data1));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  StorageInfo device_info;
  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_a, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), device_info.device_id());
  EXPECT_EQ(test_path_a.value(), device_info.location());
  EXPECT_EQ(88788ULL, device_info.total_size_in_bytes());
  EXPECT_EQ(base::ASCIIToUTF16("volume label"), device_info.storage_label());
  EXPECT_EQ(base::ASCIIToUTF16("vendor name"), device_info.vendor_name());
  EXPECT_EQ(base::ASCIIToUTF16("model name"), device_info.model_name());

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_b, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceNoDCIM), device_info.device_id());
  EXPECT_EQ(test_path_b.value(), device_info.location());

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_c, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceFixed), device_info.device_id());
  EXPECT_EQ(test_path_c.value(), device_info.location());

  // An invalid path.
  EXPECT_FALSE(notifier()->GetStorageInfoForPath(base::FilePath(kInvalidPath),
                                                 &device_info));

  // Test filling in of the mount point.
  EXPECT_TRUE(
      notifier()->GetStorageInfoForPath(test_path_a.Append("some/other/path"),
      &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), device_info.device_id());
  EXPECT_EQ(test_path_a.value(), device_info.location());

  // One device attached at multiple points.
  // kDeviceDCIM1 -> kMountPointA *
  // kDeviceFixed -> kMountPointB
  // kDeviceFixed -> kMountPointC
  MtabTestData test_data2[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceFixed, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceFixed, test_path_c.value(), kValidFS),
  };
  AppendToMtabAndRunLoop(test_data2, base::size(test_data2));

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_a, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceDCIM1), device_info.device_id());

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_b, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceFixed), device_info.device_id());

  EXPECT_TRUE(notifier()->GetStorageInfoForPath(test_path_c, &device_info));
  EXPECT_EQ(GetDeviceId(kDeviceFixed), device_info.device_id());

  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(1, observer().detach_calls());
}

TEST_F(StorageMonitorLinuxTest, DISABLED_DevicePartitionSize) {
  base::FilePath test_path_a = CreateMountPointWithDCIMDir(kMountPointA);
  base::FilePath test_path_b = CreateMountPointWithoutDCIMDir(kMountPointB);
  ASSERT_FALSE(test_path_a.empty());
  ASSERT_FALSE(test_path_b.empty());

  MtabTestData test_data1[] = {
    MtabTestData(kDeviceDCIM1, test_path_a.value(), kValidFS),
    MtabTestData(kDeviceNoDCIM, test_path_b.value(), kValidFS),
    MtabTestData(kDeviceFixed, kInvalidPath, kInvalidFS),
  };
  AppendToMtabAndRunLoop(test_data1, base::size(test_data1));
  EXPECT_EQ(2, observer().attach_calls());
  EXPECT_EQ(0, observer().detach_calls());

  EXPECT_EQ(GetDevicePartitionSize(kDeviceDCIM1),
            GetStorageSize(test_path_a));
  EXPECT_EQ(GetDevicePartitionSize(kDeviceNoDCIM),
            GetStorageSize(test_path_b));
  EXPECT_EQ(GetDevicePartitionSize(kInvalidPath),
            GetStorageSize(base::FilePath(kInvalidPath)));
}

}  // namespace

}  // namespace storage_monitor

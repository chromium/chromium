// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kImageCaptureDeviceId[] = "ic:xyz";

}  // namespace

namespace storage_monitor {

class MediaStorageUtilTest : public testing::Test {
 public:
  MediaStorageUtilTest() {}
  ~MediaStorageUtilTest() override {}

  // Verify mounted device type.
  void CheckDCIMDeviceType(const base::FilePath& mount_point) {
    EXPECT_TRUE(MediaStorageUtil::HasDcim(mount_point));
  }

  void CheckNonDCIMDeviceType(const base::FilePath& mount_point) {
    EXPECT_FALSE(MediaStorageUtil::HasDcim(mount_point));
  }

  void ProcessAttach(const std::string& id,
                     const base::FilePath::StringType& location) {
    StorageInfo info(id, location, base::string16(), base::string16(),
                     base::string16(), 0);
    monitor_->receiver()->ProcessAttach(info);
  }

 protected:
  // Create mount point for the test device.
  base::FilePath CreateMountPoint(bool create_dcim_dir) {
    base::FilePath path(scoped_temp_dir_.GetPath());
    if (create_dcim_dir)
      path = path.Append(kDCIMDirectoryName);
    if (!base::CreateDirectory(path))
      return base::FilePath();
    return scoped_temp_dir_.GetPath();
  }

  void SetUp() override {
    monitor_ = TestStorageMonitor::CreateAndInstall();
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    TestStorageMonitor::Destroy();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestStorageMonitor* monitor_;
  base::ScopedTempDir scoped_temp_dir_;
};

// Test to verify that HasDcim() function returns true for the given media
// device mount point.
TEST_F(MediaStorageUtilTest, MediaDeviceAttached) {
  // Create a dummy mount point with DCIM Directory.
  base::FilePath mount_point(CreateMountPoint(true));
  ASSERT_FALSE(mount_point.empty());
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&MediaStorageUtilTest::CheckDCIMDeviceType,
                     base::Unretained(this), mount_point));
  RunUntilIdle();
}

// Test to verify that HasDcim() function returns false for a given non-media
// device mount point.
TEST_F(MediaStorageUtilTest, NonMediaDeviceAttached) {
  // Create a dummy mount point without DCIM Directory.
  base::FilePath mount_point(CreateMountPoint(false));
  ASSERT_FALSE(mount_point.empty());
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&MediaStorageUtilTest::CheckNonDCIMDeviceType,
                     base::Unretained(this), mount_point));
  RunUntilIdle();
}

TEST_F(MediaStorageUtilTest, CanCreateFileSystemForImageCapture) {
  EXPECT_TRUE(MediaStorageUtil::CanCreateFileSystem(kImageCaptureDeviceId,
                                                    base::FilePath()));
  EXPECT_FALSE(MediaStorageUtil::CanCreateFileSystem(
      "dcim:xyz", base::FilePath()));
  EXPECT_FALSE(MediaStorageUtil::CanCreateFileSystem(
      "dcim:xyz", base::FilePath(FILE_PATH_LITERAL("relative"))));
  EXPECT_FALSE(MediaStorageUtil::CanCreateFileSystem(
      "dcim:xyz", base::FilePath(FILE_PATH_LITERAL("../refparent"))));
}

TEST_F(MediaStorageUtilTest, DetectDeviceFiltered) {
  MediaStorageUtil::DeviceIdSet devices;
  devices.insert(kImageCaptureDeviceId);

  MediaStorageUtil::FilterAttachedDevices(&devices, base::DoNothing());
  RunUntilIdle();
  EXPECT_FALSE(devices.find(kImageCaptureDeviceId) != devices.end());

  ProcessAttach(kImageCaptureDeviceId, FILE_PATH_LITERAL("/location"));
  devices.insert(kImageCaptureDeviceId);
  MediaStorageUtil::FilterAttachedDevices(&devices, base::DoNothing());
  RunUntilIdle();

  EXPECT_TRUE(devices.find(kImageCaptureDeviceId) != devices.end());
}

}  // namespace storage_monitor

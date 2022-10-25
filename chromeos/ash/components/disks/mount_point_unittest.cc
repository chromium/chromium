// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/disks/mount_point.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::WithArg;
using ::testing::WithoutArgs;

namespace ash {
namespace disks {
namespace {

constexpr char kSourcePath[] = "/source/path";
constexpr char kMountPath[] = "/mount/path";

class MountPointTest : public testing::Test {
 public:
  MountPointTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  MockDiskMountManager disk_mount_manager_;
};

TEST_F(MountPointTest, Mount) {
  EXPECT_CALL(disk_mount_manager_,
              MountPath(kSourcePath, "", "", _, MountType::kDevice,
                        MountAccessMode::kReadWrite, _))
      .WillOnce(
          RunOnceCallback<6>(MountError::kSuccess,
                             DiskMountManager::MountPoint{
                                 kSourcePath, kMountPath, MountType::kDevice}));
  EXPECT_CALL(disk_mount_manager_, UnmountPath(kMountPath, _)).Times(1);

  base::RunLoop run_loop;
  MountPoint::Mount(&disk_mount_manager_, kSourcePath, "", "", {},
                    MountType::kDevice, MountAccessMode::kReadWrite,
                    base::BindLambdaForTesting(
                        [&run_loop](MountError mount_error,
                                    std::unique_ptr<MountPoint> mount) {
                          EXPECT_EQ(MountError::kSuccess, mount_error);
                          EXPECT_EQ(kMountPath, mount->mount_path().value());
                          run_loop.Quit();
                        }));
  run_loop.Run();
}

TEST_F(MountPointTest, MountFailure) {
  EXPECT_CALL(disk_mount_manager_,
              MountPath(kSourcePath, "", "", _, MountType::kDevice,
                        MountAccessMode::kReadWrite, _))
      .WillOnce(RunOnceCallback<6>(
          MountError::kUnknownError,
          DiskMountManager::MountPoint{kSourcePath, kMountPath,
                                       MountType::kDevice,
                                       MountError::kUnsupportedFilesystem}));
  EXPECT_CALL(disk_mount_manager_, UnmountPath(_, _)).Times(0);

  base::RunLoop run_loop;
  MountPoint::Mount(&disk_mount_manager_, kSourcePath, "", "", {},
                    MountType::kDevice, MountAccessMode::kReadWrite,
                    base::BindLambdaForTesting(
                        [&run_loop](MountError mount_error,
                                    std::unique_ptr<MountPoint> mount) {
                          EXPECT_EQ(MountError::kUnknownError, mount_error);
                          EXPECT_FALSE(mount);
                          run_loop.Quit();
                        }));
  run_loop.Run();
}

TEST_F(MountPointTest, Unmount) {
  EXPECT_CALL(disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(MountError::kInternalError));

  base::RunLoop run_loop;
  MountPoint mount_point(base::FilePath(kMountPath), &disk_mount_manager_);
  mount_point.Unmount(base::BindLambdaForTesting([&run_loop](MountError error) {
    EXPECT_EQ(MountError::kInternalError, error);
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(MountPointTest, UnmountOnDestruction) {
  EXPECT_CALL(disk_mount_manager_, UnmountPath(kMountPath, _)).Times(1);

  MountPoint mount_point(base::FilePath(kMountPath), &disk_mount_manager_);
}

TEST_F(MountPointTest, UnmountThenDestory) {
  base::RunLoop run_loop;
  EXPECT_CALL(disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(WithArg<1>(
          [this, &run_loop](DiskMountManager::UnmountPathCallback callback) {
            task_environment_.GetMainThreadTaskRunner()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback),
                                          MountError::kInternalError));
            task_environment_.GetMainThreadTaskRunner()->PostTask(
                FROM_HERE, run_loop.QuitClosure());
          }));

  std::unique_ptr<MountPoint> mount_point = std::make_unique<MountPoint>(
      base::FilePath(kMountPath), &disk_mount_manager_);
  mount_point->Unmount(base::BindLambdaForTesting([](MountError error) {
    // Expect that this callback is never run.
    FAIL();
  }));
  mount_point.reset();
  run_loop.Run();
}

}  // namespace
}  // namespace disks
}  // namespace ash

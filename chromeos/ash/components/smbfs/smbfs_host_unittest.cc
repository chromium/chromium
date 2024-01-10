// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/smbfs/smbfs_host.h"

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace smbfs {
namespace {

constexpr base::FilePath::CharType kMountPath[] = FILE_PATH_LITERAL("/foo/bar");
constexpr char kUsername[] = "my-username";
constexpr char kWorkgroup[] = "my-workgroup";
constexpr char kPassword[] = "super-secret-password-shhh-dont-tell-anyone";

class MockDelegate : public SmbFsHost::Delegate {
 public:
  MOCK_METHOD(void, OnDisconnected, (), (override));
  MOCK_METHOD(void,
              RequestCredentials,
              (RequestCredentialsCallback),
              (override));
};

class SmbFsHostTest : public testing::Test {
 protected:
  void SetUp() override {
    smbfs_pending_receiver_ = smbfs_remote_.BindNewPipeAndPassReceiver();
    delegate_pending_receiver_ = delegate_remote_.BindNewPipeAndPassReceiver();
  }

  base::test::TaskEnvironment task_environment_;

  MockDelegate mock_delegate_;
  ash::disks::MockDiskMountManager mock_disk_mount_manager_;

  mojo::Remote<mojom::SmbFs> smbfs_remote_;
  mojo::PendingReceiver<mojom::SmbFs> smbfs_pending_receiver_;
  mojo::Remote<mojom::SmbFsDelegate> delegate_remote_;
  mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_pending_receiver_;
};

TEST_F(SmbFsHostTest, DisconnectDelegate) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, OnDisconnected())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(ash::MountError::kSuccess));

  std::unique_ptr<SmbFsHost> host = std::make_unique<SmbFsHost>(
      std::make_unique<ash::disks::MountPoint>(base::FilePath(kMountPath),
                                               &mock_disk_mount_manager_),
      &mock_delegate_, std::move(smbfs_remote_),
      std::move(delegate_pending_receiver_));
  delegate_remote_.reset();

  run_loop.Run();
}

TEST_F(SmbFsHostTest, DisconnectSmbFs) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, OnDisconnected())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(ash::MountError::kSuccess));

  std::unique_ptr<SmbFsHost> host = std::make_unique<SmbFsHost>(
      std::make_unique<ash::disks::MountPoint>(base::FilePath(kMountPath),
                                               &mock_disk_mount_manager_),
      &mock_delegate_, std::move(smbfs_remote_),
      std::move(delegate_pending_receiver_));
  smbfs_pending_receiver_.reset();

  run_loop.Run();
}

TEST_F(SmbFsHostTest, UnmountOnDestruction) {
  EXPECT_CALL(mock_delegate_, OnDisconnected()).Times(0);
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(ash::MountError::kSuccess));

  base::RunLoop run_loop;
  std::unique_ptr<SmbFsHost> host = std::make_unique<SmbFsHost>(
      std::make_unique<ash::disks::MountPoint>(base::FilePath(kMountPath),
                                               &mock_disk_mount_manager_),
      &mock_delegate_, std::move(smbfs_remote_),
      std::move(delegate_pending_receiver_));
  run_loop.RunUntilIdle();
  host.reset();
}

TEST_F(SmbFsHostTest, RequestCredentials_ProvideCredentials) {
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(ash::MountError::kSuccess));

  std::unique_ptr<SmbFsHost> host = std::make_unique<SmbFsHost>(
      std::make_unique<ash::disks::MountPoint>(base::FilePath(kMountPath),
                                               &mock_disk_mount_manager_),
      &mock_delegate_, std::move(smbfs_remote_),
      std::move(delegate_pending_receiver_));
  EXPECT_CALL(mock_delegate_, RequestCredentials(_))
      .WillOnce(base::test::RunOnceCallback<0>(false /* cancel */, kUsername,
                                               kWorkgroup, kPassword));

  base::RunLoop run_loop;
  delegate_remote_->RequestCredentials(base::BindLambdaForTesting(
      [&run_loop](mojom::CredentialsPtr credentials) {
        ASSERT_TRUE(credentials);
        EXPECT_EQ(credentials->username, kUsername);
        EXPECT_EQ(credentials->workgroup, kWorkgroup);
        ASSERT_TRUE(credentials->password);
        EXPECT_EQ(credentials->password->length,
                  static_cast<int32_t>(strlen(kPassword)));
        std::string password_buf(credentials->password->length, 'a');
        base::ScopedFD fd =
            mojo::UnwrapPlatformHandle(std::move(credentials->password->fd))
                .TakeFD();
        EXPECT_TRUE(base::ReadFromFD(fd.get(), password_buf));
        EXPECT_EQ(password_buf, kPassword);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SmbFsHostTest, RequestCredentials_Cancel) {
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(ash::MountError::kSuccess));

  std::unique_ptr<SmbFsHost> host = std::make_unique<SmbFsHost>(
      std::make_unique<ash::disks::MountPoint>(base::FilePath(kMountPath),
                                               &mock_disk_mount_manager_),
      &mock_delegate_, std::move(smbfs_remote_),
      std::move(delegate_pending_receiver_));
  EXPECT_CALL(mock_delegate_, RequestCredentials(_))
      .WillOnce(base::test::RunOnceCallback<0>(
          true /* cancel */, "" /* username */, "" /* workgroup */,
          "" /* password */));

  base::RunLoop run_loop;
  delegate_remote_->RequestCredentials(base::BindLambdaForTesting(
      [&run_loop](mojom::CredentialsPtr credentials) {
        EXPECT_FALSE(credentials);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace smbfs

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/smbfs/smbfs_mounter.h"

#include <string.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using testing::_;
using testing::StartsWith;
using testing::WithArgs;

namespace smbfs {
namespace {

constexpr char kMountUrlPrefix[] = "smbfs://";
constexpr char kSharePath[] = "smb://server/share";
constexpr char kMountDir[] = "bar";
constexpr base::FilePath::CharType kMountPath[] = FILE_PATH_LITERAL("/foo/bar");
constexpr int kChildInvitationFd = 42;
constexpr char kUsername[] = "username";
constexpr char kWorkgroup[] = "example.com";
constexpr char kPassword[] = "myverysecurepassword";
constexpr char kKerberosIdentity[] = "my-kerberos-identity";
constexpr char kAccountHash[] = "00112233445566778899aabb";

ash::disks::DiskMountManager::MountPoint MakeMountPointInfo(
    const std::string& source_path,
    const std::string& mount_path) {
  return {source_path, mount_path, ash::MountType::kNetworkStorage};
}

class MockDelegate : public SmbFsHost::Delegate {
 public:
  MOCK_METHOD(void, OnDisconnected, (), (override));
  MOCK_METHOD(void,
              RequestCredentials,
              (RequestCredentialsCallback),
              (override));
};

class TestSmbFsBootstrapImpl : public mojom::SmbFsBootstrap {
 public:
  MOCK_METHOD(void,
              MountShare,
              (mojom::MountOptionsPtr,
               mojo::PendingRemote<mojom::SmbFsDelegate>,
               MountShareCallback),
              (override));
};

class TestSmbFsImpl : public mojom::SmbFs {
 public:
  MOCK_METHOD(void,
              RemoveSavedCredentials,
              (RemoveSavedCredentialsCallback),
              (override));

  MOCK_METHOD(void,
              DeleteRecursively,
              (const base::FilePath&, DeleteRecursivelyCallback),
              (override));
};

class TestSmbFsMounter : public SmbFsMounter {
 public:
  TestSmbFsMounter(const std::string& share_path,
                   const MountOptions& options,
                   SmbFsHost::Delegate* delegate,
                   const base::FilePath& mount_path,
                   ash::MountError mount_error,
                   mojo::Remote<mojom::SmbFsBootstrap> bootstrap)
      : SmbFsMounter(share_path,
                     kMountDir,
                     options,
                     delegate,
                     &mock_disk_mount_manager_,
                     std::move(bootstrap)) {
    EXPECT_CALL(mock_disk_mount_manager_, MountPath(StartsWith(kMountUrlPrefix),
                                                    _, kMountDir, _, _, _, _))
        .WillOnce(WithArgs<0, 6>(
            [mount_error, mount_path](
                const std::string& source_path,
                ash::disks::DiskMountManager::MountPathCallback callback) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      std::move(callback), mount_error,
                      MakeMountPointInfo(source_path, mount_path.value())));
            }));
    if (mount_error == ash::MountError::kSuccess) {
      EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(mount_path.value(), _))
          .WillOnce(base::test::RunOnceCallback<1>(ash::MountError::kSuccess));
    } else {
      EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(mount_path.value(), _))
          .Times(0);
    }
  }

 private:
  ash::disks::MockDiskMountManager mock_disk_mount_manager_;
};

class SmbFsMounterTest : public testing::Test {
 public:
  void PostMountEvent(
      const std::string& source_path,
      const std::string& mount_path,
      ash::MountError mount_error,
      ash::disks::DiskMountManager::MountPathCallback callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), mount_error,
                                  MakeMountPointInfo(source_path, mount_path)));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockDelegate mock_delegate_;
  ash::disks::MockDiskMountManager mock_disk_mount_manager_;
};

TEST_F(SmbFsMounterTest, FilesystemMountTimeout) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kTimeout);
        EXPECT_FALSE(host);
        run_loop.Quit();
      });

  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<SmbFsMounter>(
      kSharePath, kMountDir, SmbFsMounter::MountOptions(), &mock_delegate_,
      &mock_disk_mount_manager_);
  EXPECT_CALL(mock_disk_mount_manager_,
              MountPath(StartsWith(kMountUrlPrefix), _, kMountDir, _, _, _, _))
      .Times(1);
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(_, _)).Times(0);

  base::TimeTicks start_time = task_environment_.NowTicks();
  mounter->Mount(callback);

  // TaskEnvironment will automatically advance mock time to the next posted
  // task, which is the mount timeout in this case.
  run_loop.Run();

  EXPECT_GE(task_environment_.NowTicks() - start_time, base::Seconds(20));
}

TEST_F(SmbFsMounterTest, FilesystemMountFailure) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kUnknown);
        EXPECT_FALSE(host);
        run_loop.Quit();
      });

  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<SmbFsMounter>(
      kSharePath, kMountDir, SmbFsMounter::MountOptions(), &mock_delegate_,
      &mock_disk_mount_manager_);

  EXPECT_CALL(mock_disk_mount_manager_,
              MountPath(StartsWith(kMountUrlPrefix), _, kMountDir, _, _, _, _))
      .WillOnce(WithArgs<0, 6>(
          [this](const std::string& source_path,
                 ash::disks::DiskMountManager::MountPathCallback callback) {
            PostMountEvent(source_path, kMountPath,
                           ash::MountError::kInternalError,
                           std::move(callback));
          }));
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(_, _)).Times(0);

  mounter->Mount(callback);
  run_loop.Run();
}

TEST_F(SmbFsMounterTest, TimeoutAfterFilesystemMount) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kTimeout);
        EXPECT_FALSE(host);
        run_loop.Quit();
      });

  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<SmbFsMounter>(
      kSharePath, kMountDir, SmbFsMounter::MountOptions(), &mock_delegate_,
      &mock_disk_mount_manager_);

  EXPECT_CALL(mock_disk_mount_manager_,
              MountPath(StartsWith(kMountUrlPrefix), _, kMountDir, _, _, _, _))
      .WillOnce(WithArgs<0, 6>(
          [this](const std::string& source_path,
                 ash::disks::DiskMountManager::MountPathCallback callback) {
            PostMountEvent(source_path, kMountPath, ash::MountError::kSuccess,
                           std::move(callback));
          }));
  // Destructing SmbFsMounter on failure will cause the mount point to be
  // unmounted.
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _)).Times(1);

  base::TimeTicks start_time = task_environment_.NowTicks();
  mounter->Mount(callback);

  // TaskEnvironment will automatically advance mock time to the next posted
  // task, which is the mount timeout in this case.
  run_loop.Run();

  EXPECT_GE(task_environment_.NowTicks() - start_time, base::Seconds(20));
}

TEST_F(SmbFsMounterTest, FilesystemMountAfterDestruction) {
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [](mojom::MountError mount_error, std::unique_ptr<SmbFsHost> host) {
        FAIL() << "Callback should not be run";
      });

  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<SmbFsMounter>(
      kSharePath, kMountDir, SmbFsMounter::MountOptions(), &mock_delegate_,
      &mock_disk_mount_manager_);

  EXPECT_CALL(mock_disk_mount_manager_,
              MountPath(StartsWith(kMountUrlPrefix), _, kMountDir, _, _, _, _))
      .WillOnce(WithArgs<0, 6>(
          [this](const std::string& source_path,
                 ash::disks::DiskMountManager::MountPathCallback callback) {
            // This posts a mount event to the task queue, which will not be run
            // until |run_loop| is started.
            PostMountEvent(source_path, kMountPath,
                           ash::MountError::kInternalError,
                           std::move(callback));
          }));
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(_, _)).Times(0);

  mounter->Mount(callback);

  // Delete the mounter. Callback should not be run.
  mounter.reset();

  run_loop.RunUntilIdle();
}

TEST_F(SmbFsMounterTest, MountOptions) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kOk);
        ASSERT_TRUE(host);
        EXPECT_EQ(host->mount_path(), base::FilePath(kMountPath));
        run_loop.Quit();
      });

  // Dummy Mojo bindings to satifsy lifetimes.
  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;
  TestSmbFsImpl mock_smbfs;
  mojo::Receiver<mojom::SmbFs> smbfs_receiver(&mock_smbfs);

  TestSmbFsBootstrapImpl mock_bootstrap;
  mojo::Receiver<mojom::SmbFsBootstrap> bootstrap_receiver(&mock_bootstrap);
  EXPECT_CALL(mock_bootstrap, MountShare(_, _, _))
      .WillOnce([&delegate_remote, &smbfs_receiver](
                    mojom::MountOptionsPtr options,
                    mojo::PendingRemote<mojom::SmbFsDelegate> delegate,
                    mojom::SmbFsBootstrap::MountShareCallback callback) {
        EXPECT_EQ(options->share_path, kSharePath);
        EXPECT_EQ(options->username, kUsername);
        EXPECT_EQ(options->workgroup, kWorkgroup);
        ASSERT_TRUE(options->password);
        EXPECT_EQ(options->password->length,
                  static_cast<int32_t>(strlen(kPassword)));
        std::string password_buf(options->password->length, 'a');
        base::ScopedFD fd =
            mojo::UnwrapPlatformHandle(std::move(options->password->fd))
                .TakeFD();
        EXPECT_TRUE(base::ReadFromFD(fd.get(), password_buf));
        EXPECT_EQ(password_buf, kPassword);
        EXPECT_TRUE(options->allow_ntlm);
        EXPECT_FALSE(options->skip_connect);
        EXPECT_EQ(options->resolved_host, net::IPAddress(1, 2, 3, 4));
        EXPECT_FALSE(options->credential_storage_options);

        delegate_remote = std::move(delegate);
        std::move(callback).Run(mojom::MountError::kOk,
                                smbfs_receiver.BindNewPipeAndPassRemote());
      });

  SmbFsMounter::MountOptions mount_options;
  mount_options.username = kUsername;
  mount_options.workgroup = kWorkgroup;
  mount_options.password = kPassword;
  mount_options.allow_ntlm = true;
  mount_options.resolved_host = net::IPAddress(1, 2, 3, 4);
  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<TestSmbFsMounter>(
      kSharePath, mount_options, &mock_delegate_, base::FilePath(kMountPath),
      ash::MountError::kSuccess,
      mojo::Remote<mojom::SmbFsBootstrap>(
          bootstrap_receiver.BindNewPipeAndPassRemote()));
  mounter->Mount(callback);

  run_loop.Run();
}

TEST_F(SmbFsMounterTest, MountOptions_SkipConnect) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kOk);
        ASSERT_TRUE(host);
        EXPECT_EQ(host->mount_path(), base::FilePath(kMountPath));
        run_loop.Quit();
      });

  // Dummy Mojo bindings to satisfy lifetimes.
  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;
  TestSmbFsImpl mock_smbfs;
  mojo::Receiver<mojom::SmbFs> smbfs_receiver(&mock_smbfs);

  TestSmbFsBootstrapImpl mock_bootstrap;
  mojo::Receiver<mojom::SmbFsBootstrap> bootstrap_receiver(&mock_bootstrap);
  EXPECT_CALL(mock_bootstrap, MountShare(_, _, _))
      .WillOnce([&delegate_remote, &smbfs_receiver](
                    mojom::MountOptionsPtr options,
                    mojo::PendingRemote<mojom::SmbFsDelegate> delegate,
                    mojom::SmbFsBootstrap::MountShareCallback callback) {
        EXPECT_EQ(options->share_path, kSharePath);
        EXPECT_TRUE(options->skip_connect);

        delegate_remote = std::move(delegate);
        std::move(callback).Run(mojom::MountError::kOk,
                                smbfs_receiver.BindNewPipeAndPassRemote());
      });

  SmbFsMounter::MountOptions mount_options;
  mount_options.skip_connect = true;
  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<TestSmbFsMounter>(
      kSharePath, mount_options, &mock_delegate_, base::FilePath(kMountPath),
      ash::MountError::kSuccess,
      mojo::Remote<mojom::SmbFsBootstrap>(
          bootstrap_receiver.BindNewPipeAndPassRemote()));
  mounter->Mount(callback);

  run_loop.Run();
}

TEST_F(SmbFsMounterTest, MountOptions_SavePassword) {
  // Salt must be at least 16 bytes.
  const std::vector<uint8_t> kSalt = {0, 1, 2,  3,  4,  5,  6,  7,
                                      8, 9, 10, 11, 12, 13, 14, 15};

  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kOk);
        ASSERT_TRUE(host);
        EXPECT_EQ(host->mount_path(), base::FilePath(kMountPath));
        run_loop.Quit();
      });

  // Dummy Mojo bindings to satisfy lifetimes.
  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;
  TestSmbFsImpl mock_smbfs;
  mojo::Receiver<mojom::SmbFs> smbfs_receiver(&mock_smbfs);

  TestSmbFsBootstrapImpl mock_bootstrap;
  mojo::Receiver<mojom::SmbFsBootstrap> bootstrap_receiver(&mock_bootstrap);
  EXPECT_CALL(mock_bootstrap, MountShare(_, _, _))
      .WillOnce([&delegate_remote, &smbfs_receiver, kSalt](
                    mojom::MountOptionsPtr options,
                    mojo::PendingRemote<mojom::SmbFsDelegate> delegate,
                    mojom::SmbFsBootstrap::MountShareCallback callback) {
        EXPECT_EQ(options->share_path, kSharePath);
        ASSERT_TRUE(options->credential_storage_options);
        EXPECT_EQ(options->credential_storage_options->account_hash,
                  kAccountHash);
        EXPECT_EQ(options->credential_storage_options->salt, kSalt);

        delegate_remote = std::move(delegate);
        std::move(callback).Run(mojom::MountError::kOk,
                                smbfs_receiver.BindNewPipeAndPassRemote());
      });

  SmbFsMounter::MountOptions mount_options;
  mount_options.save_restore_password = true;
  mount_options.account_hash = kAccountHash;
  mount_options.password_salt = kSalt;
  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<TestSmbFsMounter>(
      kSharePath, mount_options, &mock_delegate_, base::FilePath(kMountPath),
      ash::MountError::kSuccess,
      mojo::Remote<mojom::SmbFsBootstrap>(
          bootstrap_receiver.BindNewPipeAndPassRemote()));
  mounter->Mount(callback);

  run_loop.Run();
}

TEST_F(SmbFsMounterTest, KerberosAuthentication) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kOk);
        ASSERT_TRUE(host);
        EXPECT_EQ(host->mount_path(), base::FilePath(kMountPath));
        run_loop.Quit();
      });

  // Dummy Mojo bindings to satifsy lifetimes.
  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;
  TestSmbFsImpl mock_smbfs;
  mojo::Receiver<mojom::SmbFs> smbfs_receiver(&mock_smbfs);

  TestSmbFsBootstrapImpl mock_bootstrap;
  mojo::Receiver<mojom::SmbFsBootstrap> bootstrap_receiver(&mock_bootstrap);
  EXPECT_CALL(mock_bootstrap, MountShare(_, _, _))
      .WillOnce([&delegate_remote, &smbfs_receiver](
                    mojom::MountOptionsPtr options,
                    mojo::PendingRemote<mojom::SmbFsDelegate> delegate,
                    mojom::SmbFsBootstrap::MountShareCallback callback) {
        EXPECT_EQ(options->share_path, kSharePath);
        EXPECT_EQ(options->username, kUsername);
        EXPECT_EQ(options->workgroup, kWorkgroup);
        EXPECT_FALSE(options->allow_ntlm);
        EXPECT_FALSE(options->password);

        ASSERT_TRUE(options->kerberos_config);
        EXPECT_EQ(options->kerberos_config->source,
                  mojom::KerberosConfig::Source::kKerberos);
        EXPECT_EQ(options->kerberos_config->identity, kKerberosIdentity);

        delegate_remote = std::move(delegate);
        std::move(callback).Run(mojom::MountError::kOk,
                                smbfs_receiver.BindNewPipeAndPassRemote());
      });

  SmbFsMounter::MountOptions mount_options;
  mount_options.username = kUsername;
  mount_options.workgroup = kWorkgroup;
  // Even though the password is set, it should be ignored because kerberos
  // authentication is being used.
  mount_options.password = kPassword;
  mount_options.kerberos_options =
      std::make_optional<SmbFsMounter::KerberosOptions>(
          SmbFsMounter::KerberosOptions::Source::kKerberos, kKerberosIdentity);
  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<TestSmbFsMounter>(
      kSharePath, mount_options, &mock_delegate_, base::FilePath(kMountPath),
      ash::MountError::kSuccess,
      mojo::Remote<mojom::SmbFsBootstrap>(
          bootstrap_receiver.BindNewPipeAndPassRemote()));
  mounter->Mount(callback);

  run_loop.Run();
}

TEST_F(SmbFsMounterTest, BootstrapMountError) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kAccessDenied);
        EXPECT_FALSE(host);
        run_loop.Quit();
      });

  // Dummy Mojo bindings to satifsy lifetimes.
  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;

  TestSmbFsBootstrapImpl mock_bootstrap;
  mojo::Receiver<mojom::SmbFsBootstrap> bootstrap_receiver(&mock_bootstrap);
  EXPECT_CALL(mock_bootstrap, MountShare(_, _, _))
      .WillOnce([&delegate_remote](
                    mojom::MountOptionsPtr options,
                    mojo::PendingRemote<mojom::SmbFsDelegate> delegate,
                    mojom::SmbFsBootstrap::MountShareCallback callback) {
        delegate_remote = std::move(delegate);
        std::move(callback).Run(mojom::MountError::kAccessDenied, {});
      });

  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<TestSmbFsMounter>(
      kSharePath, SmbFsMounter::MountOptions(), &mock_delegate_,
      base::FilePath(kMountPath), ash::MountError::kSuccess,
      mojo::Remote<mojom::SmbFsBootstrap>(
          bootstrap_receiver.BindNewPipeAndPassRemote()));
  mounter->Mount(callback);

  run_loop.Run();
}

TEST_F(SmbFsMounterTest, BootstrapDisconnection) {
  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kUnknown);
        EXPECT_FALSE(host);
        run_loop.Quit();
      });

  // Dummy Mojo bindings to satifsy lifetimes.
  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;

  TestSmbFsBootstrapImpl mock_bootstrap;
  mojo::Receiver<mojom::SmbFsBootstrap> bootstrap_receiver(&mock_bootstrap);
  EXPECT_CALL(mock_bootstrap, MountShare(_, _, _))
      .WillOnce([&bootstrap_receiver](
                    mojom::MountOptionsPtr options,
                    mojo::PendingRemote<mojom::SmbFsDelegate> delegate,
                    mojom::SmbFsBootstrap::MountShareCallback callback) {
        // Reset the bootstrap binding, which should cause a disconnect event.
        bootstrap_receiver.reset();
        std::move(callback).Run(mojom::MountError::kAccessDenied, {});
      });

  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<TestSmbFsMounter>(
      kSharePath, SmbFsMounter::MountOptions(), &mock_delegate_,
      base::FilePath(kMountPath), ash::MountError::kSuccess,
      mojo::Remote<mojom::SmbFsBootstrap>(
          bootstrap_receiver.BindNewPipeAndPassRemote()));
  mounter->Mount(callback);

  run_loop.Run();
}

class SmbFsMounterE2eTest : public testing::Test {
 public:
  void PostMountEvent(
      const std::string& source_path,
      const std::string& mount_path,
      ash::disks::DiskMountManager::MountPathCallback callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ash::MountError::kSuccess,
                       MakeMountPointInfo(source_path, mount_path)));
  }

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(ipc_thread.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0)));
    ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        ipc_thread.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  }

 protected:
  // This test performs actual IPC using sockets, and therefore cannot use
  // MOCK_TIME, which automatically advances time when the main loop is idle.
  base::test::TaskEnvironment task_environment_;

  MockDelegate mock_delegate_;
  ash::disks::MockDiskMountManager mock_disk_mount_manager_;

 private:
  base::Thread ipc_thread{"IPC thread"};
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
};

// Child process that emulates the behaviour of smbfs.
MULTIPROCESS_TEST_MAIN(SmbFsMain) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED);
  mojo::core::ScopedIPCSupport ipc_support(
      task_environment.GetMainThreadTaskRunner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  mojo::IncomingInvitation invitation =
      mojo::IncomingInvitation::Accept(mojo::PlatformChannelEndpoint(
          mojo::PlatformHandle(base::ScopedFD(kChildInvitationFd))));

  TestSmbFsImpl mock_smbfs;
  mojo::Receiver<mojom::SmbFs> smbfs_receiver(&mock_smbfs);

  TestSmbFsBootstrapImpl mock_bootstrap;
  mojo::Receiver<mojom::SmbFsBootstrap> bootstrap_receiver(&mock_bootstrap);

  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;

  base::RunLoop run_loop;
  EXPECT_CALL(mock_bootstrap, MountShare(_, _, _))
      .WillOnce([&smbfs_receiver, &run_loop, &delegate_remote](
                    mojom::MountOptionsPtr options,
                    mojo::PendingRemote<mojom::SmbFsDelegate> delegate,
                    mojom::SmbFsBootstrap::MountShareCallback callback) {
        EXPECT_EQ(options->share_path, kSharePath);
        EXPECT_EQ(options->username, kUsername);
        EXPECT_EQ(options->workgroup, kWorkgroup);
        ASSERT_TRUE(options->password);
        EXPECT_EQ(options->password->length,
                  static_cast<int32_t>(strlen(kPassword)));
        std::string password_buf(options->password->length, 'a');
        base::ScopedFD fd =
            mojo::UnwrapPlatformHandle(std::move(options->password->fd))
                .TakeFD();
        EXPECT_TRUE(base::ReadFromFD(fd.get(), password_buf));
        EXPECT_EQ(password_buf, kPassword);

        EXPECT_FALSE(options->allow_ntlm);

        delegate_remote = std::move(delegate);
        mojo::PendingRemote<mojom::SmbFs> smbfs =
            smbfs_receiver.BindNewPipeAndPassRemote();
        // When the SmbFsHost in the parent is destroyed, this message pipe will
        // be closed and treat that as a signal to shut down.
        smbfs_receiver.set_disconnect_handler(run_loop.QuitClosure());

        std::move(callback).Run(mojom::MountError::kOk, std::move(smbfs));
      });

  bootstrap_receiver.Bind(mojo::PendingReceiver<mojom::SmbFsBootstrap>(
      invitation.ExtractMessagePipe(mojom::kBootstrapPipeName)));

  run_loop.Run();

  return 0;
}

TEST_F(SmbFsMounterE2eTest, MountSuccess) {
  mojo::PlatformChannel channel;

  base::LaunchOptions launch_options;
  base::ScopedFD child_fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();
  launch_options.fds_to_remap.push_back(
      std::make_pair(child_fd.get(), kChildInvitationFd));
  base::Process child_process = base::SpawnMultiProcessTestChild(
      "SmbFsMain", base::GetMultiProcessTestChildBaseCommandLine(),
      launch_options);
  ASSERT_TRUE(child_process.IsValid());
  // The child FD has been passed to the child process at this point.
  std::ignore = child_fd.release();

  EXPECT_CALL(mock_disk_mount_manager_,
              MountPath(StartsWith(kMountUrlPrefix), _, kMountDir, _, _, _, _))
      .WillOnce(WithArgs<0,
                         6>([this, &channel](
                                const std::string& source_path,
                                ash::disks::DiskMountManager::MountPathCallback
                                    callback) {
        // Emulates cros-disks mount success.
        PostMountEvent(source_path, kMountPath, std::move(callback));

        // Emulates smbfs connecting to the org.chromium.SmbFs D-Bus service and
        // providing a Mojo connection endpoint.
        const std::string token =
            source_path.substr(sizeof(kMountUrlPrefix) - 1);
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindLambdaForTesting([token, &channel]() {
              mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
                  token,
                  channel.TakeLocalEndpoint().TakePlatformHandle().TakeFD());
            }));
      }));
  EXPECT_CALL(mock_disk_mount_manager_, UnmountPath(kMountPath, _))
      .WillOnce(base::test::RunOnceCallback<1>(ash::MountError::kSuccess));
  EXPECT_CALL(mock_delegate_, OnDisconnected()).Times(0);

  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&run_loop](mojom::MountError mount_error,
                                             std::unique_ptr<SmbFsHost> host) {
        EXPECT_EQ(mount_error, mojom::MountError::kOk);
        EXPECT_TRUE(host);
        // Don't capture |host|. Its destruction will close the Mojo message
        // pipe and cause the child process to shut down gracefully.
        run_loop.Quit();
      });

  SmbFsMounter::MountOptions mount_options;
  mount_options.username = kUsername;
  mount_options.workgroup = kWorkgroup;
  mount_options.password = kPassword;
  std::unique_ptr<SmbFsMounter> mounter = std::make_unique<SmbFsMounter>(
      kSharePath, kMountDir, mount_options, &mock_delegate_,
      &mock_disk_mount_manager_);
  mounter->Mount(callback);

  run_loop.Run();

  EXPECT_TRUE(child_process.WaitForExit(nullptr));
}

}  // namespace
}  // namespace smbfs

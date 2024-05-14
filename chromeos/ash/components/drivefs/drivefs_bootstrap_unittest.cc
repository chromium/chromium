// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_bootstrap.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-test-utils.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

using testing::_;

class MockDriveFs : public mojom::DriveFsInterceptorForTesting {
 public:
  DriveFs* GetForwardingInterface() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
};

class MockDriveFsDelegate : public mojom::DriveFsDelegateInterceptorForTesting {
 public:
  DriveFsDelegate* GetForwardingInterface() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
};

class DriveFsBootstrapListenerForTest : public DriveFsBootstrapListener {
 public:
  DriveFsBootstrapListenerForTest(
      mojo::PendingRemote<mojom::DriveFsBootstrap> available_bootstrap)
      : available_bootstrap_(std::move(available_bootstrap)) {}

  DriveFsBootstrapListenerForTest(const DriveFsBootstrapListenerForTest&) =
      delete;
  DriveFsBootstrapListenerForTest& operator=(
      const DriveFsBootstrapListenerForTest&) = delete;

  mojo::PendingRemote<mojom::DriveFsBootstrap> bootstrap() override {
    return std::move(available_bootstrap_);
  }

  void SendInvitationOverPipe(base::ScopedFD) override {}

 private:
  mojo::PendingRemote<mojom::DriveFsBootstrap> available_bootstrap_;
};

class DriveFsBootstrapTest : public testing::Test,
                             public mojom::DriveFsBootstrap {
 public:
  DriveFsBootstrapTest() = default;

  DriveFsBootstrapTest(const DriveFsBootstrapTest&) = delete;
  DriveFsBootstrapTest& operator=(const DriveFsBootstrapTest&) = delete;

 protected:
  MOCK_CONST_METHOD0(OnDisconnect, void());
  MOCK_CONST_METHOD0(OnInit, void());

  void Init(mojom::DriveFsConfigurationPtr config,
            mojo::PendingReceiver<mojom::DriveFs> drive_fs_receiver,
            mojo::PendingRemote<mojom::DriveFsDelegate> delegate) override {
    receiver_.Bind(std::move(drive_fs_receiver));
    mojo::FusePipes(std::move(pending_delegate_receiver_), std::move(delegate));
    OnInit();
  }

  std::unique_ptr<DriveFsBootstrapListener> CreateListener() {
    pending_delegate_receiver_ = delegate_.BindNewPipeAndPassReceiver();
    return std::make_unique<DriveFsBootstrapListenerForTest>(
        bootstrap_receiver_.BindNewPipeAndPassRemote());
  }

  base::UnguessableToken ListenForConnection() {
    connection_ = DriveFsConnection::Create(CreateListener(),
                                            mojom::DriveFsConfiguration::New());
    return connection_->Connect(
        &mock_delegate_, base::BindOnce(&DriveFsBootstrapTest::OnDisconnect,
                                        base::Unretained(this)));
  }

  void WaitForConnection(const base::UnguessableToken& token) {
    ASSERT_TRUE(mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
        token.ToString(), {}));
    base::RunLoop run_loop;
    bootstrap_receiver_.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  MockDriveFs mock_drivefs_;
  MockDriveFsDelegate mock_delegate_;

  mojo::Receiver<mojom::DriveFsBootstrap> bootstrap_receiver_{this};
  mojo::Receiver<mojom::DriveFs> receiver_{&mock_drivefs_};
  std::unique_ptr<DriveFsConnection> connection_;
  mojo::Remote<mojom::DriveFsDelegate> delegate_;
  mojo::PendingReceiver<mojom::DriveFsDelegate> pending_delegate_receiver_;
  std::string email_;
};

}  // namespace

TEST_F(DriveFsBootstrapTest, Listen_Connect_Disconnect) {
  auto token = ListenForConnection();
  EXPECT_CALL(*this, OnInit());
  WaitForConnection(token);
  EXPECT_CALL(*this, OnDisconnect());
  receiver_.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
      token.ToString(), {}));
}

TEST_F(DriveFsBootstrapTest, Listen_Connect_DisconnectDelegate) {
  auto token = ListenForConnection();
  EXPECT_CALL(*this, OnInit());
  WaitForConnection(token);
  EXPECT_CALL(*this, OnDisconnect());
  delegate_.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
      token.ToString(), {}));
}

TEST_F(DriveFsBootstrapTest, Listen_Connect_Destroy) {
  auto token = ListenForConnection();
  EXPECT_CALL(*this, OnInit());
  WaitForConnection(token);
  EXPECT_CALL(*this, OnDisconnect()).Times(0);
  connection_.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
      token.ToString(), {}));
}

TEST_F(DriveFsBootstrapTest, Listen_Destroy) {
  EXPECT_CALL(*this, OnDisconnect()).Times(0);
  auto token = ListenForConnection();
  connection_.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
      token.ToString(), {}));
}

TEST_F(DriveFsBootstrapTest, Listen_DisconnectDelegate) {
  EXPECT_CALL(*this, OnDisconnect()).Times(0);
  ListenForConnection();
  delegate_.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace drivefs

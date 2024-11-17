// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_server_socket.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_device.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {

const char kDeviceAddress[] = "DeviceAddress";
const char kDeviceName[] = "DeviceName";

class FakeSocket : public bluetooth::mojom::Socket {
 public:
  FakeSocket(mojo::ScopedDataPipeProducerHandle receive_stream,
             mojo::ScopedDataPipeConsumerHandle send_stream)
      : receive_stream_(std::move(receive_stream)),
        send_stream_(std::move(send_stream)) {}
  ~FakeSocket() override = default;

 private:
  // bluetooth::mojom::Socket:
  void Disconnect(DisconnectCallback callback) override {
    std::move(callback).Run();
  }

  mojo::ScopedDataPipeProducerHandle receive_stream_;
  mojo::ScopedDataPipeConsumerHandle send_stream_;
};

class FakeServerSocket : public bluetooth::mojom::ServerSocket {
 public:
  FakeServerSocket() = default;
  ~FakeServerSocket() override {
    if (on_destroy_callback_)
      std::move(on_destroy_callback_).Run();
  }

  void SetAcceptConnectionResult(
      bluetooth::mojom::AcceptConnectionResultPtr result) {
    accept_connection_result_ = std::move(result);
  }

  void SetOnDestroyCallback(base::OnceClosure callback) {
    on_destroy_callback_ = std::move(callback);
  }

  void SetAcceptShouldBlock(bool block) { accept_should_block_ = block; }

 private:
  // bluetooth::mojom::ServerSocket:
  void Accept(AcceptCallback callback) override {
    if (accept_should_block_) {
      accept_callback_ = std::move(callback);
      return;
    }
    std::move(callback).Run(std::move(accept_connection_result_));
  }
  void Disconnect(DisconnectCallback callback) override {
    if (accept_callback_) {
      std::move(accept_callback_).Run(nullptr);
    }
    std::move(callback).Run();
  }

  AcceptCallback accept_callback_;
  bluetooth::mojom::AcceptConnectionResultPtr accept_connection_result_;
  base::OnceClosure on_destroy_callback_;
  bool accept_should_block_;
};

}  // namespace

class BluetoothServerSocketTest : public testing::Test {
 public:
  BluetoothServerSocketTest() = default;
  ~BluetoothServerSocketTest() override = default;
  BluetoothServerSocketTest(const BluetoothServerSocketTest&) = delete;
  BluetoothServerSocketTest& operator=(const BluetoothServerSocketTest&) =
      delete;

  void SetUp() override {
    auto fake_server_socket = std::make_unique<FakeServerSocket>();
    fake_server_socket_ = fake_server_socket.get();

    mojo::PendingRemote<bluetooth::mojom::ServerSocket> pending_server_socket;

    mojo::MakeSelfOwnedReceiver(
        std::move(fake_server_socket),
        pending_server_socket.InitWithNewPipeAndPassReceiver());

    // In production Nearby Connections code, BluetoothServerSocket is created
    // on a thread separate from the thread it is later used on. Replicate this
    // behavior by creating |bluetooth_server_socket_| on a dedicated thread.
    base::RunLoop run_loop;
    auto creation_task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock()}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
    creation_task_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          bluetooth_server_socket_ = std::make_unique<BluetoothServerSocket>(
              std::move(pending_server_socket));
          run_loop.Quit();
        }));
    run_loop.Run();
  }

 protected:
  raw_ptr<FakeServerSocket, DanglingUntriaged> fake_server_socket_ = nullptr;

  std::unique_ptr<BluetoothServerSocket> bluetooth_server_socket_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BluetoothServerSocketTest, TestAccept_Success) {
  auto accept_connection_result =
      bluetooth::mojom::AcceptConnectionResult::New();

  accept_connection_result->device = bluetooth::mojom::DeviceInfo::New();
  accept_connection_result->device->address = kDeviceAddress;
  accept_connection_result->device->name_for_display = kDeviceName;

  mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(/*options=*/nullptr,
                                                 receive_pipe_producer_handle,
                                                 receive_pipe_consumer_handle));

  mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(/*options=*/nullptr, send_pipe_producer_handle,
                                 send_pipe_consumer_handle));

  mojo::PendingRemote<bluetooth::mojom::Socket> pending_socket;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FakeSocket>(std::move(receive_pipe_producer_handle),
                                   std::move(send_pipe_consumer_handle)),
      pending_socket.InitWithNewPipeAndPassReceiver());

  accept_connection_result->socket = std::move(pending_socket);
  accept_connection_result->receive_stream =
      std::move(receive_pipe_consumer_handle);
  accept_connection_result->send_stream = std::move(send_pipe_producer_handle);

  fake_server_socket_->SetAcceptConnectionResult(
      std::move(accept_connection_result));

  auto bluetooth_socket = bluetooth_server_socket_->Accept();
  ASSERT_TRUE(bluetooth_socket);
  EXPECT_EQ(kDeviceName, bluetooth_socket->GetRemoteDevice()->GetName());
}

TEST_F(BluetoothServerSocketTest, TestAccept_NoConnection) {
  EXPECT_FALSE(bluetooth_server_socket_->Accept());
}

TEST_F(BluetoothServerSocketTest, TestCloseInterruptsBlockingAccept) {
  base::RunLoop run_loop;
  fake_server_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  fake_server_socket_->SetAcceptShouldBlock(true);
  auto close_socket_task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock()},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  close_socket_task_runner->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        bluetooth_server_socket_->Close();
      }),
      base::Seconds(1));

  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
  EXPECT_FALSE(bluetooth_server_socket_->Accept());
  run_loop.Run();
}

TEST_F(BluetoothServerSocketTest, TestClose) {
  base::RunLoop run_loop;
  fake_server_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  bluetooth_server_socket_->Close();
  run_loop.Run();
}

TEST_F(BluetoothServerSocketTest, TestCloseThenAccept) {
  base::RunLoop run_loop;
  fake_server_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  bluetooth_server_socket_->Close();
  run_loop.Run();
  ASSERT_FALSE(bluetooth_server_socket_->Accept());
}

TEST_F(BluetoothServerSocketTest, TestDestroy) {
  base::RunLoop run_loop;
  fake_server_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  bluetooth_server_socket_.reset();
  run_loop.Run();
}

}  // namespace nearby::chrome

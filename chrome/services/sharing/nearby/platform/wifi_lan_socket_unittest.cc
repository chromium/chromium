// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_lan_socket.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_connected_socket.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class WifiLanSocketTest : public ::testing::Test {
 public:
  WifiLanSocketTest() = default;
  ~WifiLanSocketTest() override = default;
  WifiLanSocketTest(const WifiLanSocketTest&) = delete;
  WifiLanSocketTest& operator=(const WifiLanSocketTest&) = delete;

  void SetUp() override {
    mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
    ASSERT_EQ(
        MOJO_RESULT_OK,
        mojo::CreateDataPipe(/*options=*/nullptr, receive_pipe_producer_handle,
                             receive_pipe_consumer_handle));
    mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(/*options=*/nullptr,
                                                   send_pipe_producer_handle,
                                                   send_pipe_consumer_handle));

    auto fake_tcp_connected_socket =
        std::make_unique<ash::nearby::FakeTcpConnectedSocket>(
            std::move(receive_pipe_producer_handle),
            std::move(send_pipe_consumer_handle));
    fake_tcp_connected_socket_ = fake_tcp_connected_socket.get();
    mojo::PendingRemote<network::mojom::TCPConnectedSocket>
        tcp_connected_socket;
    tcp_connected_socket_self_owned_receiver_ref_ = mojo::MakeSelfOwnedReceiver(
        std::move(fake_tcp_connected_socket),
        tcp_connected_socket.InitWithNewPipeAndPassReceiver());

    wifi_lan_socket_ = std::make_unique<WifiLanSocket>(
        WifiLanSocket::ConnectedSocketParameters(
            std::move(tcp_connected_socket),
            std::move(receive_pipe_consumer_handle),
            std::move(send_pipe_producer_handle)));
  }

  void TearDown() override { wifi_lan_socket_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::ScopedDataPipeProducerHandle receive_stream_;
  mojo::ScopedDataPipeConsumerHandle send_stream_;
  raw_ptr<ash::nearby::FakeTcpConnectedSocket, DanglingUntriaged>
      fake_tcp_connected_socket_;
  mojo::SelfOwnedReceiverRef<network::mojom::TCPConnectedSocket>
      tcp_connected_socket_self_owned_receiver_ref_;
  std::unique_ptr<WifiLanSocket> wifi_lan_socket_;
};

TEST_F(WifiLanSocketTest, Close) {
  base::RunLoop run_loop;
  fake_tcp_connected_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  EXPECT_TRUE(wifi_lan_socket_->Close().Ok());
  run_loop.Run();

  EXPECT_TRUE(wifi_lan_socket_->IsClosed());

  // Ensure that calls to Close() succeed even after the underlying socket is
  // destroyed.
  EXPECT_TRUE(wifi_lan_socket_->Close().Ok());
}

TEST_F(WifiLanSocketTest, Close_CalledFromMultipleThreads) {
  base::RunLoop run_loop;
  const size_t kNumThreads = 4;
  // Quit the run loop after Close() returns on all threads.
  size_t num_close_calls = 0;
  auto quit_callback =
      base::BindLambdaForTesting([&num_close_calls, &run_loop] {
        ++num_close_calls;
        if (num_close_calls == kNumThreads)
          run_loop.Quit();
      });

  // Call Close() from different threads simultaneously to ensure the socket is
  // shutdown gracefully.
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([this] {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow;
          EXPECT_EQ(Exception::kSuccess, wifi_lan_socket_->Close().value);
        }),
        quit_callback);
  }
  run_loop.Run();
}

TEST_F(WifiLanSocketTest, Destroy) {
  base::RunLoop run_loop;
  fake_tcp_connected_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  wifi_lan_socket_.reset();
  run_loop.Run();
}

TEST_F(WifiLanSocketTest, Disconnect) {
  base::RunLoop run_loop;
  fake_tcp_connected_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  tcp_connected_socket_self_owned_receiver_ref_->Close();
  run_loop.Run();

  // Check that the WifiLanSocket's disconnect handler was triggered.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(wifi_lan_socket_->IsClosed());
}

}  // namespace nearby::chrome

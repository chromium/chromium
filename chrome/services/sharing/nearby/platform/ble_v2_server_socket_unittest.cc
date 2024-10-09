// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_server_socket.h"

#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/services/sharing/nearby/platform/count_down_latch.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

class BleV2ServerSocketTest : public testing::Test {
 public:
  BleV2ServerSocketTest() = default;
  ~BleV2ServerSocketTest() override = default;
  BleV2ServerSocketTest(const BleV2ServerSocketTest&) = delete;
  BleV2ServerSocketTest& operator=(const BleV2ServerSocketTest&) = delete;

  void SetUp() override {
    ble_v2_socket_ = std::make_unique<BleV2Socket>();
    ble_v2_server_socket_ = std::make_unique<BleV2ServerSocket>();

    accept_socket_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock()}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);

    close_socket_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock()}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }

  void ServerSocketAccept(base::RunLoop& run_loop, CountDownLatch& latch) {
    accept_socket_task_runner_->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
          // Accept always returns a nullptr. Expect this to block.
          EXPECT_EQ(nullptr, ble_v2_server_socket_->Accept());

          // Decrement the latch after Accept exits.
          latch.CountDown();
          run_loop.Quit();
        }));
  }

  void PostDelayedServerSocketClose() {
    close_socket_task_runner_->PostDelayedTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
          EXPECT_TRUE(ble_v2_server_socket_->Close().Ok());
        }),
        base::Seconds(1));
  }

 protected:
  scoped_refptr<base::SingleThreadTaskRunner> accept_socket_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> close_socket_task_runner_;
  std::unique_ptr<BleV2Socket> ble_v2_socket_;
  std::unique_ptr<BleV2ServerSocket> ble_v2_server_socket_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BleV2ServerSocketTest, ServerSocket_AcceptAndClose) {
  // Calls Close() after 1s.
  PostDelayedServerSocketClose();

  // Server Socket's Accept is designed to wait forever until
  // Close is called, as the calling thread in Nearby Connections
  // calls accept in a while true loop that can cause crashes
  // and out-of-memory errors otherwise. This mimics what a true
  // implementation would do, which is block until a Bluetooth
  // connection is ready for socket creation.
  CountDownLatch accept_latch(1);
  base::RunLoop run_loop;
  ServerSocketAccept(run_loop, accept_latch);
  run_loop.Run();

  // Ensure Accept exits without error, after Close is called.
  EXPECT_TRUE(accept_latch.Await().Ok());
}

TEST_F(BleV2ServerSocketTest, ServerSocket_MultipleAcceptAndClose) {
  // Calling Close multiple times should cause no errors.
  PostDelayedServerSocketClose();
  PostDelayedServerSocketClose();
  PostDelayedServerSocketClose();

  // Calling Accept multiple times should cause no errors.
  CountDownLatch accept_latch(3);
  base::RunLoop run_loop_1;
  ServerSocketAccept(run_loop_1, accept_latch);
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  ServerSocketAccept(run_loop_2, accept_latch);
  run_loop_2.Run();

  base::RunLoop run_loop_3;
  ServerSocketAccept(run_loop_3, accept_latch);
  run_loop_3.Run();

  // Ensure all Accept tasks exit without error, after Close is called.
  EXPECT_TRUE(accept_latch.Await().Ok());
}

TEST_F(BleV2ServerSocketTest, ServerSocket_AcceptDoesntBlockAfterClose) {
  // Calling Close before Accept causes no error,
  // and Accept will not block.
  EXPECT_TRUE(ble_v2_server_socket_->Close().Ok());
  EXPECT_EQ(nullptr, ble_v2_server_socket_->Accept());
}

TEST_F(BleV2ServerSocketTest, Socket_Close) {
  EXPECT_TRUE(ble_v2_socket_->Close().Ok());
}

TEST_F(BleV2ServerSocketTest, Socket_GetRemotePeripheral) {
  EXPECT_TRUE(ble_v2_socket_->GetRemotePeripheral());
}

TEST_F(BleV2ServerSocketTest, InputStream_Read) {
  InputStream* input_stream = &ble_v2_socket_->GetInputStream();
  EXPECT_FALSE(input_stream->Read(1u).ok());
  EXPECT_TRUE(input_stream->Read(1u).GetException().Raised(Exception::kIo));
}

TEST_F(BleV2ServerSocketTest, InputStream_ReadExactly) {
  InputStream* input_stream = &ble_v2_socket_->GetInputStream();
  EXPECT_FALSE(input_stream->ReadExactly(1u).ok());
  EXPECT_TRUE(
      input_stream->ReadExactly(1u).GetException().Raised(Exception::kIo));
}

TEST_F(BleV2ServerSocketTest, InputStream_Close) {
  InputStream* input_stream = &ble_v2_socket_->GetInputStream();
  EXPECT_TRUE(input_stream->Close().Ok());
}

TEST_F(BleV2ServerSocketTest, OutputStream_Write) {
  OutputStream* output_stream = &ble_v2_socket_->GetOutputStream();
  EXPECT_TRUE(output_stream->Write(ByteArray()).Raised(Exception::kIo));
}

TEST_F(BleV2ServerSocketTest, OutputStream_Flush) {
  OutputStream* output_stream = &ble_v2_socket_->GetOutputStream();
  EXPECT_TRUE(output_stream->Flush().Ok());
}

TEST_F(BleV2ServerSocketTest, OutputStream_Close) {
  OutputStream* output_stream = &ble_v2_socket_->GetOutputStream();
  EXPECT_TRUE(output_stream->Close().Ok());
}

}  // namespace nearby::chrome

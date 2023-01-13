// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bidirectional_stream.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/services/sharing/nearby/platform/input_stream_impl.h"
#include "chrome/services/sharing/nearby/platform/output_stream_impl.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby {
namespace chrome {

class BidirectionalStreamTest : public ::testing::Test {
 public:
  BidirectionalStreamTest()
      : task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}
  ~BidirectionalStreamTest() override = default;
  BidirectionalStreamTest(const BidirectionalStreamTest&) = delete;
  BidirectionalStreamTest& operator=(const BidirectionalStreamTest&) = delete;

  void SetUp() override {
    mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
    ASSERT_EQ(
        MOJO_RESULT_OK,
        mojo::CreateDataPipe(/*options=*/nullptr, receive_pipe_producer_handle,
                             receive_pipe_consumer_handle));
    receive_stream_ = std::move(receive_pipe_producer_handle);

    mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(/*options=*/nullptr,
                                                   send_pipe_producer_handle,
                                                   send_pipe_consumer_handle));
    send_stream_ = std::move(send_pipe_consumer_handle);

    bidirectional_stream_ = std::make_unique<BidirectionalStream>(
        connections::mojom::Medium::kBluetooth, task_runner_,
        std::move(receive_pipe_consumer_handle),
        std::move(send_pipe_producer_handle));
    EXPECT_TRUE(bidirectional_stream_->GetInputStream());
    EXPECT_TRUE(bidirectional_stream_->GetOutputStream());
  }

  void TearDown() override { bidirectional_stream_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::ScopedDataPipeProducerHandle receive_stream_;
  mojo::ScopedDataPipeConsumerHandle send_stream_;
  std::unique_ptr<BidirectionalStream> bidirectional_stream_;
};

TEST_F(BidirectionalStreamTest, Read) {
  std::string message = "ReceivedMessage";
  uint32_t message_size = message.size();
  EXPECT_EQ(MOJO_RESULT_OK,
            receive_stream_->WriteData(message.data(), &message_size,
                                       MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(
      Exception::kSuccess,
      bidirectional_stream_->GetInputStream()->Read(message_size).exception());

  // Can't read after streams are closed
  EXPECT_EQ(MOJO_RESULT_OK,
            receive_stream_->WriteData(message.data(), &message_size,
                                       MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(Exception::kSuccess, bidirectional_stream_->Close().value);
  EXPECT_EQ(
      Exception::kIo,
      bidirectional_stream_->GetInputStream()->Read(message_size).exception());
}

TEST_F(BidirectionalStreamTest, Write) {
  std::string message = "SentMessage";
  EXPECT_EQ(Exception::kSuccess, bidirectional_stream_->GetOutputStream()
                                     ->Write(ByteArray{message})
                                     .value);
  const uint32_t max_buffer_size = 1024;
  uint32_t buffer_size = max_buffer_size;
  std::vector<char> buffer(max_buffer_size);
  EXPECT_EQ(MOJO_RESULT_OK, send_stream_->ReadData(buffer.data(), &buffer_size,
                                                   MOJO_READ_DATA_FLAG_NONE));

  // Can't write after streams are closed
  EXPECT_EQ(Exception::kSuccess, bidirectional_stream_->Close().value);
  EXPECT_EQ(Exception::kIo, bidirectional_stream_->GetOutputStream()
                                ->Write(ByteArray{message})
                                .value);
}

TEST_F(BidirectionalStreamTest, CloseCalledFromMultipleThreads) {
  base::RunLoop run_loop;

  const size_t kNumThreads = 2;

  // Quit the run loop after Close() returns on all threads.
  size_t num_close_calls = 0;
  auto quit_callback =
      base::BindLambdaForTesting([&num_close_calls, &run_loop] {
        ++num_close_calls;
        if (num_close_calls == kNumThreads)
          run_loop.Quit();
      });

  // Call Close() from different threads simultaneously to ensure the stream is
  // shutdown gracefully.
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([this] {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow;
          EXPECT_EQ(Exception::kSuccess, bidirectional_stream_->Close().value);
        }),
        quit_callback);
  }
  run_loop.Run();
}

}  // namespace chrome
}  // namespace nearby

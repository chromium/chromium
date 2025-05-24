// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/input_stream_impl.h"

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {

// Writes |message| to |receive_stream| in chunks defined by the underlying mojo
// pipe. Must be called on a background thread as this will block until all data
// has been written to the pipe.
void WriteDataBlocking(const std::string& message,
                       mojo::ScopedDataPipeProducerHandle* receive_stream) {
  mojo::ScopedDataPipeProducerHandle& stream = *receive_stream;
  base::span<const uint8_t> bytes = base::as_byte_span(message);
  while (!bytes.empty()) {
    size_t bytes_written = 0;
    MojoResult result =
        stream->WriteData(bytes, MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
    // |result| might be MOJO_RESULT_SHOULD_WAIT in which
    // case we need to retry until the reader has emptied
    // the mojo pipe enough.
    if (result == MOJO_RESULT_OK) {
      bytes = bytes.subspan(bytes_written);
    }
  }
}

}  // namespace

class InputStreamImplTest : public ::testing::Test {
 public:
  InputStreamImplTest()
      : task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}
  ~InputStreamImplTest() override = default;
  InputStreamImplTest(const InputStreamImplTest&) = delete;
  InputStreamImplTest& operator=(const InputStreamImplTest&) = delete;

  void SetUp() override {
    mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
    ASSERT_EQ(
        MOJO_RESULT_OK,
        mojo::CreateDataPipe(/*options=*/nullptr, receive_pipe_producer_handle,
                             receive_pipe_consumer_handle));

    // InputStreamImpl requires construction on |task_runner_|.
    base::RunLoop run_loop;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindLambdaForTesting([this, &receive_pipe_consumer_handle] {
          input_stream_ = std::make_unique<InputStreamImpl>(
              connections::mojom::Medium::kBluetooth, task_runner_,
              std::move(receive_pipe_consumer_handle));
        }),
        run_loop.QuitClosure());
    run_loop.Run();

    receive_stream_ = std::move(receive_pipe_producer_handle);
  }

  void TearDown() override {
    // InputStreamImpl requires destruction on |task_runner_|.
    base::RunLoop run_loop;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindLambdaForTesting([this] { input_stream_.reset(); }),
        run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::ScopedDataPipeProducerHandle receive_stream_;
  std::unique_ptr<InputStream> input_stream_;
};

TEST_F(InputStreamImplTest, Read) {
  std::string message = "ReceivedMessage";
  size_t bytes_written = 0;
  EXPECT_EQ(MOJO_RESULT_OK, receive_stream_->WriteData(
                                base::as_byte_span(message),
                                MOJO_WRITE_DATA_FLAG_NONE, bytes_written));
  EXPECT_EQ(message.size(), bytes_written);

  ExceptionOr<ByteArray> exception_or_byte_array =
      input_stream_->Read(message.size());
  ASSERT_TRUE(exception_or_byte_array.ok());

  ByteArray& byte_array = exception_or_byte_array.result();
  std::string received_string(byte_array);
  EXPECT_EQ(message, received_string);

  EXPECT_EQ(Exception::kSuccess, input_stream_->Close().value);
}

TEST_F(InputStreamImplTest, MultipleChunks) {
  // Expect a total message size of 1MB delivered in chunks because a mojo pipe
  // has a maximum buffer size and only accepts a certain amount of data per
  // call. The default is 64KB defined in //mojo/core/core.cc
  uint32_t message_size = 1024 * 1024;
  std::string message(message_size, 'A');

  // Post to a thread pool because both InputStream::Read() and
  // WriteDataBlocking() below are blocking on each other.
  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&WriteDataBlocking, message, &receive_stream_),
      run_loop.QuitClosure());

  // Read from stream and expect to receive 1MB.
  ExceptionOr<ByteArray> exception_or_byte_array =
      input_stream_->Read(message_size);
  ASSERT_TRUE(exception_or_byte_array.ok());
  EXPECT_EQ(message, std::string(exception_or_byte_array.result()));
  EXPECT_EQ(Exception::kSuccess, input_stream_->Close().value);

  // Make sure writer thread is done after we read all the data from it.
  run_loop.Run();
}

TEST_F(InputStreamImplTest, CloseBeforeRead) {
  EXPECT_EQ(Exception::kSuccess, input_stream_->Close().value);
  EXPECT_EQ(Exception::kIo, input_stream_->Read(1u).exception());
}

TEST_F(InputStreamImplTest, CloseWhileReading) {
  base::RunLoop run_loop;

  // Start waiting for 1 byte to be read from the |receive_stream_|. Note: We
  // run on a separate thread because Read() is blocking.
  ExceptionOr<ByteArray> read_exception_or_byte_array;
  base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindLambdaForTesting([this, &read_exception_or_byte_array] {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        read_exception_or_byte_array = input_stream_->Read(1u);
      }),
      run_loop.QuitClosure());

  // While Read() is waiting, close the stream. Note: We delay closing the
  // stream by 100 ms to ensure that Read() is in fact waiting when Close() is
  // posted. Because Read() is blocking, I think this is the best we can do.
  // Even if Close() somehow completes before Read(), an IO exception should
  // still be thrown.
  base::ThreadPool::CreateSequencedTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([this] {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_EQ(Exception::kSuccess, input_stream_->Close().value);
      }),
      base::Milliseconds(100));

  run_loop.Run();

  EXPECT_EQ(Exception::kIo, read_exception_or_byte_array.exception());
}

TEST_F(InputStreamImplTest, CloseCalledFromMultipleThreads) {
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
          EXPECT_EQ(Exception::kSuccess, input_stream_->Close().value);
        }),
        quit_callback);
  }
  run_loop.Run();
}

TEST_F(InputStreamImplTest, ResetHandle) {
  // Setup a message to receive that would work if the connection was not reset.
  std::string message = "ReceivedMessage";
  size_t bytes_written = 0;
  EXPECT_EQ(MOJO_RESULT_OK, receive_stream_->WriteData(
                                base::as_byte_span(message),
                                MOJO_WRITE_DATA_FLAG_NONE, bytes_written));
  EXPECT_EQ(message.size(), bytes_written);

  // Reset the pipe on the other side to trigger a peer_reset state.
  receive_stream_.reset();

  ExceptionOr<ByteArray> exception_or_byte_array =
      input_stream_->Read(message.size());
  ASSERT_FALSE(exception_or_byte_array.ok());
  EXPECT_EQ(Exception::kIo, exception_or_byte_array.exception());
}

}  // namespace nearby::chrome

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/output_stream_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {

// Tries to read |expected_message| from |send_stream| in chunks defined by the
// underlying mojo pipe. This will read exactly |expected_message.size()| bytes
// from the pipe and compare the bytes to |expected_message|. Must be called on
// a background thread as this will block until all data has been read from the
// stream.
void ReadDataBlocking(const std::string& expected_message,
                      mojo::ScopedDataPipeConsumerHandle* send_stream) {
  mojo::ScopedDataPipeConsumerHandle& stream = *send_stream;
  std::string message(expected_message.size(), '\0');
  base::span<uint8_t> buffer = base::as_writable_byte_span(message);
  while (!buffer.empty()) {
    size_t bytes_read = 0;
    MojoResult result =
        stream->ReadData(MOJO_READ_DATA_FLAG_NONE, buffer, bytes_read);
    // |result| might be MOJO_RESULT_SHOULD_WAIT in which
    // case we need to retry until the writer has filled
    // the mojo pipe again.
    if (result == MOJO_RESULT_OK) {
      buffer = buffer.subspan(bytes_read);
    }
  }
  EXPECT_EQ(expected_message, message.data());
}

}  // namespace

class OutputStreamImplTest : public ::testing::Test {
 public:
  OutputStreamImplTest()
      : task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}
  ~OutputStreamImplTest() override = default;
  OutputStreamImplTest(const OutputStreamImplTest&) = delete;
  OutputStreamImplTest& operator=(const OutputStreamImplTest&) = delete;

  void SetUp() override {
    mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
    mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
    ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(/*options=*/nullptr,
                                                   send_pipe_producer_handle,
                                                   send_pipe_consumer_handle));

    // OutputStreamImpl requires construction on |task_runner_|.
    base::RunLoop run_loop;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindLambdaForTesting([this, &send_pipe_producer_handle] {
          output_stream_ = std::make_unique<OutputStreamImpl>(
              connections::mojom::Medium::kBluetooth, task_runner_,
              std::move(send_pipe_producer_handle));
        }),
        run_loop.QuitClosure());
    run_loop.Run();

    send_stream_ = std::move(send_pipe_consumer_handle);
  }

  void TearDown() override {
    // OutputStreamImpl requires destruction on |task_runner_|.
    base::RunLoop run_loop;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindLambdaForTesting([this] { output_stream_.reset(); }),
        run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::ScopedDataPipeConsumerHandle send_stream_;
  std::unique_ptr<OutputStreamImpl> output_stream_;
};

TEST_F(OutputStreamImplTest, Write) {
  std::string message = "SentMessage";
  ByteArray byte_array(message);
  EXPECT_EQ(Exception::kSuccess, output_stream_->Write(byte_array).value);

  size_t bytes_read = 0;
  std::vector<char> buffer(1024);
  EXPECT_EQ(
      MOJO_RESULT_OK,
      send_stream_->ReadData(MOJO_READ_DATA_FLAG_NONE,
                             base::as_writable_byte_span(buffer), bytes_read));

  std::string_view sent_string =
      base::as_string_view(buffer).substr(0, bytes_read);
  EXPECT_EQ(message, sent_string);

  EXPECT_EQ(Exception::kSuccess, output_stream_->Flush().value);
  EXPECT_EQ(Exception::kSuccess, output_stream_->Close().value);
}

TEST_F(OutputStreamImplTest, MultipleChunks) {
  // Expect a total message size of 1MB delivered in chunks because a mojo pipe
  // has a maximum buffer size and only accepts a certain amount of data per
  // call. The default is 64KB defined in //mojo/core/core_ipcz.cc
  uint32_t message_size = 1024 * 1024;
  std::string message(message_size, 'A');

  // Post to a thread pool because both OutputStream::Write() and
  // ReadDataBlocking() below are blocking on each other.
  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&ReadDataBlocking, message, &send_stream_),
      run_loop.QuitClosure());

  // Write to stream and expect a successful transfer.
  EXPECT_EQ(Exception::kSuccess,
            output_stream_->Write(ByteArray(message)).value);
  EXPECT_EQ(Exception::kSuccess, output_stream_->Flush().value);
  EXPECT_EQ(Exception::kSuccess, output_stream_->Close().value);

  // Make sure reader thread is done after we wrote all the data to it.
  run_loop.Run();
}

TEST_F(OutputStreamImplTest, CloseBeforeWrite) {
  EXPECT_EQ(Exception::kSuccess, output_stream_->Close().value);
  EXPECT_EQ(Exception::kIo, output_stream_->Write(ByteArray("message")).value);
}

TEST_F(OutputStreamImplTest, CloseWhileWriting) {
  base::RunLoop run_loop;

  // Start waiting for the bytes to be written from the |send_stream_|. Note: We
  // run on a separate thread because Write() is blocking.
  Exception write_exception;
  base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([this, &write_exception] {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        // Expect a total message size of 1MB delivered in chunks because a mojo
        // pipe has a maximum buffer size and only accepts a certain amount of
        // data per call. The default is 64KB defined in
        // //mojo/core/core_ipcz.cc. We want a large message so the Write() will
        // be forced to wait.
        uint32_t message_size = 1024 * 1024;
        std::string message(message_size, 'A');
        write_exception = output_stream_->Write(ByteArray(message));
      }),
      run_loop.QuitClosure());

  // While Write() is waiting, close the stream. Note: We delay closing the
  // stream by 100 ms to ensure that Write() is in fact waiting when Close() is
  // posted. Because Write() is blocking, I think this is the best we can do.
  // Even if Close() somehow completes before Write(), an IO exception should
  // still be thrown.
  base::ThreadPool::CreateSequencedTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([this] {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_EQ(Exception::kSuccess, output_stream_->Close().value);
      }),
      base::Milliseconds(100));

  run_loop.Run();

  EXPECT_EQ(Exception::kIo, write_exception.value);
}

TEST_F(OutputStreamImplTest, CloseCalledFromMultipleThreads) {
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
          EXPECT_EQ(Exception::kSuccess, output_stream_->Close().value);
        }),
        quit_callback);
  }
  run_loop.Run();
}

TEST_F(OutputStreamImplTest, ResetHandle) {
  // Reset the pipe on the other side to trigger a peer_reset state.
  send_stream_.reset();

  std::string message = "SentMessage";
  ByteArray byte_array(message);
  EXPECT_EQ(Exception::kIo, output_stream_->Write(byte_array).value);
}

TEST_F(OutputStreamImplTest, WriteEmptyByteArray) {
  EXPECT_EQ(Exception::kSuccess, output_stream_->Write(ByteArray()).value);
  EXPECT_EQ(Exception::kSuccess, output_stream_->Close().value);
}

TEST_F(OutputStreamImplTest, WaitForInFlightWrites) {
  base::WaitableEvent write_pending;
  base::WaitableEvent write_finished;
  Exception write_result;
  output_stream_->SetOnWritePendingForTesting(base::BindOnce(
      &base::WaitableEvent::Signal, base::Unretained(&write_pending)));

  constexpr uint32_t kMessageSize = 1024 * 1024;
  ByteArray payload(std::string(kMessageSize, 'A'));

  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                   base::ScopedAllowBaseSyncPrimitivesForTesting allow;
                   write_result = output_stream_->Write(payload);
                   write_finished.Signal();
                 }));

  write_pending.Wait();
  base::WaitableEvent task_runner_flushed;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                base::Unretained(&task_runner_flushed)));
  task_runner_flushed.Wait();

  EXPECT_EQ(Exception::kSuccess, output_stream_->Close().value);
  output_stream_->WaitForInFlightWrites();
  // Subsequent calls with no in-flight writes return immediately.
  output_stream_->WaitForInFlightWrites();

  write_finished.Wait();
  EXPECT_EQ(Exception::kIo, write_result.value);
}

TEST_F(OutputStreamImplTest, ConcurrentWriteSupersedesPendingWrite) {
  base::WaitableEvent write1_pending;
  base::WaitableEvent write1_finished;
  Exception write1_result;
  output_stream_->SetOnWritePendingForTesting(base::BindOnce(
      &base::WaitableEvent::Signal, base::Unretained(&write1_pending)));

  constexpr uint32_t kMessageSize = 1024 * 1024;
  ByteArray payload1(std::string(kMessageSize, 'A'));

  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                   base::ScopedAllowBaseSyncPrimitivesForTesting allow;
                   write1_result = output_stream_->Write(payload1);
                   write1_finished.Signal();
                 }));

  write1_pending.Wait();
  base::WaitableEvent task_runner_flushed;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                base::Unretained(&task_runner_flushed)));
  task_runner_flushed.Wait();

  base::WaitableEvent write2_pending;
  base::WaitableEvent write2_finished;
  Exception write2_result;
  output_stream_->SetOnWritePendingForTesting(base::BindOnce(
      &base::WaitableEvent::Signal, base::Unretained(&write2_pending)));

  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                   base::ScopedAllowBaseSyncPrimitivesForTesting allow;
                   write2_result = output_stream_->Write(ByteArray("second"));
                   write2_finished.Signal();
                 }));

  write1_finished.Wait();
  write2_pending.Wait();
  EXPECT_EQ(Exception::kIo, write1_result.value);

  std::string received;
  std::vector<char> chunk(64 * 1024);
  while (!write2_finished.IsSignaled()) {
    size_t bytes_read = 0;
    MojoResult result =
        send_stream_->ReadData(MOJO_READ_DATA_FLAG_NONE,
                               base::as_writable_byte_span(chunk), bytes_read);
    if (result == MOJO_RESULT_OK) {
      received.append(chunk.data(), bytes_read);
    }
  }
  write2_finished.Wait();
  while (true) {
    size_t bytes_read = 0;
    MojoResult result =
        send_stream_->ReadData(MOJO_READ_DATA_FLAG_NONE,
                               base::as_writable_byte_span(chunk), bytes_read);
    if (result != MOJO_RESULT_OK || bytes_read == 0) {
      break;
    }
    received.append(chunk.data(), bytes_read);
  }

  EXPECT_EQ(Exception::kSuccess, write2_result.value);
  EXPECT_TRUE(received.ends_with("second"));
}

TEST_F(OutputStreamImplTest, ConcurrentWriteAndClose) {
  auto writer_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  auto closer_runner =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

  // Larger than the default 64KB Mojo data pipe capacity so Write() cannot
  // finish without a consumer draining the pipe.
  constexpr uint32_t kMessageSize = 128 * 1024;
  ByteArray payload(std::string(kMessageSize, 'A'));

  // Repeatedly race Write() and Close() across threads to verify no lost-signal
  // deadlock can occur regardless of StartWrite()/DoClose() ordering.
  for (int i = 0; i < 25; ++i) {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(/*options=*/nullptr, producer, consumer));

    std::unique_ptr<OutputStreamImpl> stream;
    base::WaitableEvent created;
    task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                             stream = std::make_unique<OutputStreamImpl>(
                                 connections::mojom::Medium::kBluetooth,
                                 task_runner_, std::move(producer));
                             created.Signal();
                           }));
    created.Wait();

    base::WaitableEvent writer_ready;
    base::WaitableEvent closer_ready;
    base::WaitableEvent start;
    base::WaitableEvent write_done;
    base::WaitableEvent close_done;
    Exception write_result;

    writer_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow;
          writer_ready.Signal();
          start.Wait();
          write_result = stream->Write(payload);
          write_done.Signal();
        }));
    closer_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow;
          closer_ready.Signal();
          start.Wait();
          EXPECT_EQ(Exception::kSuccess, stream->Close().value);
          close_done.Signal();
        }));

    writer_ready.Wait();
    closer_ready.Wait();
    start.Signal();

    write_done.Wait();
    close_done.Wait();
    EXPECT_EQ(Exception::kIo, write_result.value);

    base::WaitableEvent destroyed;
    task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                             stream.reset();
                             destroyed.Signal();
                           }));
    destroyed.Wait();
  }
}

}  // namespace nearby::chrome

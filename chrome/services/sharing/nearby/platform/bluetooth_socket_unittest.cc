// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_socket.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace location {
namespace nearby {
namespace chrome {

namespace {

const char kDeviceAddress1[] = "DeviceAddress1";
const char kDeviceName1[] = "DeviceName1";

class FakeSocket : public bluetooth::mojom::Socket {
 public:
  FakeSocket() = default;
  ~FakeSocket() override {
    EXPECT_TRUE(called_disconnect_);
    if (on_destroy_callback_)
      std::move(on_destroy_callback_).Run();
  }

  void SetOnDestroyCallback(base::OnceClosure callback) {
    on_destroy_callback_ = std::move(callback);
  }

 private:
  // bluetooth::mojom::Socket:
  void Disconnect(DisconnectCallback callback) override {
    called_disconnect_ = true;
    std::move(callback).Run();
  }

  bool called_disconnect_ = false;
  base::OnceClosure on_destroy_callback_;
};

// Writes |message| to |receive_stream| in chunks defined by the underlying mojo
// pipe. Must be called on a background thread as this will block until all data
// has been written to the pipe.
void WriteDataBlocking(const std::string& message,
                       mojo::ScopedDataPipeProducerHandle* receive_stream) {
  mojo::ScopedDataPipeProducerHandle& stream = *receive_stream;
  uint32_t message_pos = 0;
  while (message_pos < message.size()) {
    uint32_t written_size = message.size() - message_pos;
    MojoResult result = stream->WriteData(
        message.data() + message_pos, &written_size, MOJO_WRITE_DATA_FLAG_NONE);
    // |result| might be MOJO_RESULT_SHOULD_WAIT in which
    // case we need to retry until the reader has emptied
    // the mojo pipe enough.
    if (result == MOJO_RESULT_OK)
      message_pos += written_size;
  }
  EXPECT_EQ(message.size(), message_pos);
}

// Tries to read |expected_message| from |send_stream| in chunks defined by the
// underlying mojo pipe. This will read exactly |expected_message.size()| bytes
// from the pipe and compare the bytes to |expected_message|. Must be called on
// a background thread as this will block until all data has been read from the
// stream.
void ReadDataBlocking(const std::string& expected_message,
                      mojo::ScopedDataPipeConsumerHandle* send_stream) {
  mojo::ScopedDataPipeConsumerHandle& stream = *send_stream;
  std::vector<char> message(expected_message.size());
  uint32_t message_pos = 0;
  while (message_pos < message.size()) {
    uint32_t read_size = message.size() - message_pos;
    MojoResult result = stream->ReadData(message.data() + message_pos,
                                         &read_size, MOJO_READ_DATA_FLAG_NONE);
    // |result| might be MOJO_RESULT_SHOULD_WAIT in which
    // case we need to retry until the writer has filled
    // the mojo pipe again.
    if (result == MOJO_RESULT_OK)
      message_pos += read_size;
  }
  EXPECT_EQ(message.size(), message_pos);
  EXPECT_EQ(expected_message, std::string(message.data(), message.size()));
}

}  // namespace

class BluetoothSocketTest : public testing::Test {
 public:
  BluetoothSocketTest() = default;
  ~BluetoothSocketTest() override = default;
  BluetoothSocketTest(const BluetoothSocketTest&) = delete;
  BluetoothSocketTest& operator=(const BluetoothSocketTest&) = delete;

  void SetUp() override {
    bluetooth_device_ = std::make_unique<chrome::BluetoothDevice>(
        CreateDeviceInfo(kDeviceAddress1, kDeviceName1));
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

    auto fake_socket = std::make_unique<FakeSocket>();
    fake_socket_ = fake_socket.get();

    mojo::PendingRemote<bluetooth::mojom::Socket> pending_socket;

    mojo::MakeSelfOwnedReceiver(
        std::move(fake_socket),
        pending_socket.InitWithNewPipeAndPassReceiver());

    bluetooth_socket_ = std::make_unique<BluetoothSocket>(
        *bluetooth_device_, std::move(pending_socket),
        std::move(receive_pipe_consumer_handle),
        std::move(send_pipe_producer_handle));

    receive_stream_ = std::move(receive_pipe_producer_handle);
    send_stream_ = std::move(send_pipe_consumer_handle);
  }

  void TearDown() override {
    // Destroy here, not in BluetoothSocketTest's destructor, because this is
    // blocking.
    base::ScopedAllowBaseSyncPrimitivesForTesting testing_;
    bluetooth_socket_.reset();
  }

 protected:
  std::unique_ptr<chrome::BluetoothDevice> bluetooth_device_;
  FakeSocket* fake_socket_ = nullptr;

  std::unique_ptr<BluetoothSocket> bluetooth_socket_;

  mojo::ScopedDataPipeProducerHandle receive_stream_;
  mojo::ScopedDataPipeConsumerHandle send_stream_;

 private:
  bluetooth::mojom::DeviceInfoPtr CreateDeviceInfo(const std::string& address,
                                                   const std::string& name) {
    auto device_info = bluetooth::mojom::DeviceInfo::New();
    device_info->address = address;
    device_info->name = name;
    device_info->name_for_display = name;
    return device_info;
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(BluetoothSocketTest, TestGetRemoteDevice) {
  EXPECT_EQ(bluetooth_device_.get(), bluetooth_socket_->GetRemoteDevice());
}

TEST_F(BluetoothSocketTest, TestClose) {
  ASSERT_TRUE(fake_socket_);

  base::RunLoop run_loop;
  fake_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  EXPECT_TRUE(bluetooth_socket_->Close().Ok());
  run_loop.Run();

  // Ensure that calls to Close() succeed even after the underlying socket is
  // destroyed.
  EXPECT_TRUE(bluetooth_socket_->Close().Ok());
}

TEST_F(BluetoothSocketTest, TestDestroy) {
  ASSERT_TRUE(fake_socket_);

  base::RunLoop run_loop;
  fake_socket_->SetOnDestroyCallback(run_loop.QuitClosure());
  bluetooth_socket_.reset();
  run_loop.Run();
}

TEST_F(BluetoothSocketTest, TestInputStream) {
  InputStream& input_stream = bluetooth_socket_->GetInputStream();

  std::string message = "ReceivedMessage";
  uint32_t message_size = message.size();
  EXPECT_EQ(MOJO_RESULT_OK,
            receive_stream_->WriteData(message.data(), &message_size,
                                       MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(message.size(), message_size);

  ExceptionOr<ByteArray> exception_or_byte_array =
      input_stream.Read(message_size);
  ASSERT_TRUE(exception_or_byte_array.ok());

  ByteArray& byte_array = exception_or_byte_array.result();
  std::string received_string(byte_array);
  EXPECT_EQ(message, received_string);

  EXPECT_EQ(Exception::kSuccess, input_stream.Close().value);
}

TEST_F(BluetoothSocketTest, TestInputStream_MultipleChunks) {
  InputStream& input_stream = bluetooth_socket_->GetInputStream();

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
      input_stream.Read(message_size);
  ASSERT_TRUE(exception_or_byte_array.ok());
  EXPECT_EQ(message, std::string(exception_or_byte_array.result()));
  EXPECT_EQ(Exception::kSuccess, input_stream.Close().value);

  // Make sure writer thread is done after we read all the data from it.
  run_loop.Run();
}

TEST_F(BluetoothSocketTest, TestInputStream_CloseBeforeRead) {
  InputStream& input_stream = bluetooth_socket_->GetInputStream();
  EXPECT_EQ(Exception::kSuccess, input_stream.Close().value);
  EXPECT_EQ(Exception::kIo, input_stream.Read(1u).exception());
}

TEST_F(BluetoothSocketTest, TestInputStream_CloseWhileReading) {
  InputStream& input_stream = bluetooth_socket_->GetInputStream();

  base::RunLoop run_loop;

  // Start waiting for 1 byte to be read from the |receive_stream_|. Note: We
  // run on a separate thread because Read() is blocking.
  ExceptionOr<ByteArray> read_exception_or_byte_array;
  base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindLambdaForTesting(
          [&input_stream, &read_exception_or_byte_array] {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow;
            read_exception_or_byte_array = input_stream.Read(1u);
          }),
      run_loop.QuitClosure());

  // While Read() is waiting, close the stream. Note: We delay closing the
  // stream by 100 ms to ensure that Read() is in fact waiting when Close() is
  // posted. Because Read() is blocking, I think this is the best we can do.
  // Even if Close() somehow completes before Read(), an IO exception should
  // still be thrown.
  base::ThreadPool::CreateSequencedTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&input_stream] {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_EQ(Exception::kSuccess, input_stream.Close().value);
      }),
      base::TimeDelta::FromMilliseconds(100));

  run_loop.Run();

  EXPECT_EQ(Exception::kIo, read_exception_or_byte_array.exception());
}

TEST_F(BluetoothSocketTest, TestOutputStream) {
  OutputStream& output_stream = bluetooth_socket_->GetOutputStream();

  std::string message = "SentMessage";
  ByteArray byte_array(message);
  EXPECT_EQ(Exception::kSuccess, output_stream.Write(byte_array).value);

  const uint32_t max_buffer_size = 1024;
  uint32_t buffer_size = max_buffer_size;
  std::vector<char> buffer(max_buffer_size);
  EXPECT_EQ(MOJO_RESULT_OK, send_stream_->ReadData(buffer.data(), &buffer_size,
                                                   MOJO_READ_DATA_FLAG_NONE));

  std::string sent_string(buffer.data(), buffer_size);
  EXPECT_EQ(message, sent_string);

  EXPECT_EQ(Exception::kSuccess, output_stream.Flush().value);
  EXPECT_EQ(Exception::kSuccess, output_stream.Close().value);
}

TEST_F(BluetoothSocketTest, TestOutputStream_MultipleChunks) {
  OutputStream& output_stream = bluetooth_socket_->GetOutputStream();

  // Expect a total message size of 1MB delivered in chunks because a mojo pipe
  // has a maximum buffer size and only accepts a certain amount of data per
  // call. The default is 64KB defined in //mojo/core/core.cc
  uint32_t message_size = 1024 * 1024;
  std::string message(message_size, 'A');

  // Post to a thread pool because both InputStream::Write() and
  // ReadDataBlocking() below are blocking on each other.
  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&ReadDataBlocking, message, &send_stream_),
      run_loop.QuitClosure());

  // Write to stream and expect a succcessful transfer.
  EXPECT_EQ(Exception::kSuccess, output_stream.Write(ByteArray(message)).value);
  EXPECT_EQ(Exception::kSuccess, output_stream.Flush().value);
  EXPECT_EQ(Exception::kSuccess, output_stream.Close().value);

  // Make sure reader thread is done after we wrote all the data to it.
  run_loop.Run();
}

TEST_F(BluetoothSocketTest, TestOutputStream_CloseBeforeWrite) {
  OutputStream& output_stream = bluetooth_socket_->GetOutputStream();
  EXPECT_EQ(Exception::kSuccess, output_stream.Close().value);
  EXPECT_EQ(Exception::kIo, output_stream.Write(ByteArray("message")).value);
}

TEST_F(BluetoothSocketTest, TestOutputStream_CloseWhileWriting) {
  OutputStream& output_stream = bluetooth_socket_->GetOutputStream();

  base::RunLoop run_loop;

  // Start waiting for the bytes to be written from the |send_stream_|. Note: We
  // run on a separate thread because Write() is blocking.
  Exception write_exception;
  base::ThreadPool::CreateSequencedTaskRunner({})->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&output_stream, &write_exception] {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        // Expect a total message size of 1MB delivered in chunks because a mojo
        // pipe has a maximum buffer size and only accepts a certain amount of
        // data per call. The default is 64KB defined in //mojo/core/core.cc. We
        // want a large message so the Write() will be forced to wait.
        uint32_t message_size = 1024 * 1024;
        std::string message(message_size, 'A');
        write_exception = output_stream.Write(ByteArray(message));
      }),
      run_loop.QuitClosure());

  // While Write() is waiting, close the stream. Note: We delay closing the
  // stream by 100 ms to ensure that Write() is in fact waiting when Close() is
  // posted. Because Write() is blocking, I think this is the best we can do.
  // Even if Close() somehow completes before Write(), an IO exception should
  // still be thrown.
  base::ThreadPool::CreateSequencedTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindLambdaForTesting([&output_stream] {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_EQ(Exception::kSuccess, output_stream.Close().value);
      }),
      base::TimeDelta::FromMilliseconds(100));

  run_loop.Run();

  EXPECT_EQ(Exception::kIo, write_exception.value);
}

TEST_F(BluetoothSocketTest, TestInputStreamResetHandler) {
  InputStream& input_stream = bluetooth_socket_->GetInputStream();

  // Setup a message to receive that would work if the connection was not reset.
  std::string message = "ReceivedMessage";
  uint32_t message_size = message.size();
  EXPECT_EQ(MOJO_RESULT_OK,
            receive_stream_->WriteData(message.data(), &message_size,
                                       MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(message.size(), message_size);

  // Reset the pipe on the other side to trigger a peer_reset state.
  receive_stream_.reset();

  ExceptionOr<ByteArray> exception_or_byte_array =
      input_stream.Read(message_size);
  ASSERT_FALSE(exception_or_byte_array.ok());
  EXPECT_EQ(Exception::kIo, exception_or_byte_array.exception());
}

TEST_F(BluetoothSocketTest, TestOutputStreamResetHandling) {
  OutputStream& output_stream = bluetooth_socket_->GetOutputStream();

  // Reset the pipe on the other side to trigger a peer_reset state.
  send_stream_.reset();

  std::string message = "SentMessage";
  ByteArray byte_array(message);
  EXPECT_EQ(Exception::kIo, output_stream.Write(byte_array).value);
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/sharing/nearby/platform/wifi_direct_socket.h"

#include <algorithm>

#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::vector<uint8_t> kTestData = {0x01, 0x02, 0x03, 0x04};

constexpr char kReadResultMetricName[] =
    "Nearby.Connections.WifiDirect.Socket.Read.Result";
constexpr char kWriteResultMetricName[] =
    "Nearby.Connections.WifiDirect.Socket.Write.Result";

void RunOnTaskRunner(base::OnceClosure task) {
  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(FROM_HERE, std::move(task), run_loop.QuitClosure());
  run_loop.Run();
}

nearby::ByteArray ToByteArray(const std::vector<uint8_t>& expected_data) {
  return nearby::ByteArray(
      std::string(expected_data.begin(), expected_data.end()));
}

class FakeStreamSocket : public net::StreamSocket {
 public:
  ~FakeStreamSocket() override = default;

  const std::vector<uint8_t>& GetWriteData() { return write_data_; }
  void SetReadData(std::vector<uint8_t> data) { data_to_read_ = data; }
  void SetReadError(int error) { read_error_ = error; }

  // net::Socket
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    if (read_error_) {
      return read_error_.value();
    }

    auto bytes_to_write = std::max(uint(buf_len), uint(data_to_read_.size()));
    std::copy(data_to_read_.data(), data_to_read_.data() + bytes_to_write,
              buf->data());
    return bytes_to_write;
  }

  int ReadIfReady(net::IOBuffer* buf,
                  int buf_len,
                  net::CompletionOnceCallback callback) override {
    return net::ERR_NOT_IMPLEMENTED;
  }

  int CancelReadIfReady() override { return net::ERR_NOT_IMPLEMENTED; }

  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    write_data_ = std::vector(buf->bytes(), buf->bytes() + buf_len);
    return buf_len;
  }

  int SetReceiveBufferSize(int32_t size) override {
    return net::ERR_NOT_IMPLEMENTED;
  }

  int SetSendBufferSize(int32_t size) override {
    return net::ERR_NOT_IMPLEMENTED;
  }

  // net::StreamSocket
  void SetBeforeConnectCallback(
      const BeforeConnectCallback& before_connect_callback) override {}

  int Connect(net::CompletionOnceCallback callback) override {
    return net::ERR_NOT_IMPLEMENTED;
  }

  void Disconnect() override {}

  bool IsConnected() const override { return false; }

  bool IsConnectedAndIdle() const override { return false; }

  int GetLocalAddress(net::IPEndPoint* address) const override {
    return net::ERR_NOT_IMPLEMENTED;
  }

  int GetPeerAddress(net::IPEndPoint* address) const override {
    return net::ERR_NOT_IMPLEMENTED;
  }

  bool WasEverUsed() const override { return false; }

  net::NextProto GetNegotiatedProtocol() const override {
    return net::NextProto::kProtoUnknown;
  }

  bool GetSSLInfo(net::SSLInfo* ssl_info) override { return false; }

  void ApplySocketTag(const net::SocketTag& tag) override {}

  const net::NetLogWithSource& NetLog() const override { return net_log_; }

  int64_t GetTotalReceivedBytes() const override { return 0; }

 private:
  net::NetLogWithSource net_log_;
  std::vector<uint8_t> write_data_;
  std::vector<uint8_t> data_to_read_;
  std::optional<int> read_error_;
};

}  // namespace

namespace nearby::chrome {

class WifiDirectSocketTest : public ::testing::Test {
 public:
  // ::testing::Test
  void SetUp() override {
    io_thread_ = std::make_unique<base::Thread>("wifi-direct-socket-test");
    io_thread_->StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));

    auto stream_socket = std::make_unique<FakeStreamSocket>();

    // Create the subject under test.
    socket_ = std::make_unique<WifiDirectSocket>(io_thread_->task_runner(),
                                                 std::move(stream_socket));
  }

  // ::testing::Test
  void TearDown() override {
    socket_.reset();
    io_thread_->Stop();
  }

  WifiDirectSocket* socket() { return socket_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<WifiDirectSocket> socket_;
};

TEST_F(WifiDirectSocketTest, Close) {
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectSocket* socket) { EXPECT_TRUE(socket->Close()); },
      socket()));
}

TEST_F(WifiDirectSocketTest, Close_MultipleCalls) {
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectSocket* socket) {
        EXPECT_TRUE(socket->Close());
        EXPECT_FALSE(socket->Close());
      },
      socket()));
}

// SocketInputStream
class SocketInputStreamTest : public ::testing::Test {
 public:
  // ::testing::Test
  void SetUp() override {
    stream_socket_ = std::make_unique<FakeStreamSocket>();
    input_stream_ = std::make_unique<SocketInputStream>(
        stream_socket_.get(), task_environment_.GetMainThreadTaskRunner());
  }

  SocketInputStream* input_stream() { return input_stream_.get(); }
  FakeStreamSocket* stream_socket() { return stream_socket_.get(); }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<FakeStreamSocket> stream_socket_;
  std::unique_ptr<SocketInputStream> input_stream_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SocketInputStreamTest, Read) {
  stream_socket()->SetReadData(kTestData);
  histogram_tester().ExpectTotalCount(kReadResultMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](SocketInputStream* input_stream) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        auto result = input_stream->Read(kTestData.size());
        EXPECT_TRUE(result.ok());
        EXPECT_EQ(result.GetResult(), ToByteArray(kTestData));
      },
      input_stream()));

  histogram_tester().ExpectTotalCount(kReadResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kReadResultMetricName,
                                       /*bucket:true=*/1, 1);
}

TEST_F(SocketInputStreamTest, Read_Error) {
  stream_socket()->SetReadError(net::ERR_FAILED);
  histogram_tester().ExpectTotalCount(kReadResultMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](SocketInputStream* input_stream) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        auto result = input_stream->Read(1);
        EXPECT_FALSE(result.ok());
        EXPECT_EQ(result.GetException(), Exception{Exception::kFailed});
      },
      input_stream()));

  histogram_tester().ExpectTotalCount(kReadResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kReadResultMetricName,
                                       /*bucket:false=*/0, 1);
}

TEST_F(SocketInputStreamTest, Read_AfterClose) {
  stream_socket()->SetReadData(kTestData);
  histogram_tester().ExpectTotalCount(kReadResultMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](SocketInputStream* input_stream) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        input_stream->Close();
        auto result = input_stream->Read(1);
        EXPECT_FALSE(result.ok());
        EXPECT_EQ(result.GetException(), Exception{Exception::kFailed});
      },
      input_stream()));

  histogram_tester().ExpectTotalCount(kReadResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kReadResultMetricName,
                                       /*bucket:false=*/0, 1);
}

// SocketOutputStream
class SocketOutputStreamTest : public ::testing::Test {
 public:
  // ::testing::Test
  void SetUp() override {
    stream_socket_ = std::make_unique<FakeStreamSocket>();
    output_stream_ = std::make_unique<SocketOutputStream>(
        stream_socket_.get(), task_environment_.GetMainThreadTaskRunner());
  }

  SocketOutputStream* output_stream() { return output_stream_.get(); }
  FakeStreamSocket* stream_socket() { return stream_socket_.get(); }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<FakeStreamSocket> stream_socket_;
  std::unique_ptr<SocketOutputStream> output_stream_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SocketOutputStreamTest, Write) {
  histogram_tester().ExpectTotalCount(kWriteResultMetricName, 0);

  RunOnTaskRunner(base::BindOnce(
      [](SocketOutputStream* output_stream, FakeStreamSocket* socket) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;

        auto result = output_stream->Write(ToByteArray(kTestData));
        EXPECT_TRUE(result.Ok());
        EXPECT_EQ(socket->GetWriteData(), kTestData);
      },
      output_stream(), stream_socket()));

  histogram_tester().ExpectTotalCount(kWriteResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kWriteResultMetricName,
                                       /*bucket:true=*/1, 1);
}

TEST_F(SocketOutputStreamTest, Write_AfterClose) {
  RunOnTaskRunner(base::BindOnce(
      [](SocketOutputStream* output_stream, FakeStreamSocket* socket) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        output_stream->Close();
        auto result = output_stream->Write(ToByteArray(kTestData));
        EXPECT_FALSE(result.Ok());
        EXPECT_TRUE(socket->GetWriteData().empty());
      },
      output_stream(), stream_socket()));

  histogram_tester().ExpectTotalCount(kWriteResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kWriteResultMetricName,
                                       /*bucket:false=*/0, 1);
}

}  // namespace nearby::chrome

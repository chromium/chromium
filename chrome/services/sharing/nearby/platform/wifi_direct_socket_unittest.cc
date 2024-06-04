// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_socket.h"

#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeStreamSocket : public net::StreamSocket {
 public:
  ~FakeStreamSocket() override = default;

  // net::Socket
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override {
    return net::ERR_NOT_IMPLEMENTED;
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
    return net::ERR_NOT_IMPLEMENTED;
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

  void RunOnTaskRunner(base::OnceClosure task) {
    base::RunLoop run_loop;
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTaskAndReply(FROM_HERE, std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

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

}  // namespace nearby::chrome

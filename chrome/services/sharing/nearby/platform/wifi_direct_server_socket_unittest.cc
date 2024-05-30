// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_server_socket.h"

#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestIPv4Address[] = "127.0.0.1";
constexpr uint16_t kMinPort = 49152;  // Arbitrary port used for WifiLan.
constexpr uint16_t kMaxPort = 65535;

}  // namespace

namespace nearby::chrome {

class WifiDirectServerSocketTest : public ::testing::Test {
 public:
  // ::testing::Test
  void SetUp() override {
    auto tcp_server_socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());

    std::optional<net::IPAddress> address =
        net::IPAddress::FromIPLiteral(kTestIPv4Address);
    // Pick a random port for each test run, otherwise the `Listen` call below
    // has a chance to return ADDRESS_IN_USE(-147).
    uint16_t port = static_cast<uint16_t>(
        kMinPort + base::RandGenerator(kMaxPort - kMinPort + 1));
    net::IPEndPoint end_point(*address, port);
    EXPECT_EQ(
        tcp_server_socket->Listen(end_point,
                                  /*backlog=*/1, /*ipv6_only=*/std::nullopt),
        net::OK);

    // Create the subject under test.
    server_socket_ = std::make_unique<WifiDirectServerSocket>(
        task_environment_.GetMainThreadTaskRunner(), mojo::PlatformHandle(),
        std::move(tcp_server_socket));
  }

  WifiDirectServerSocket* socket() { return server_socket_.get(); }

  void RunOnTaskRunner(base::OnceClosure task) {
    base::RunLoop run_loop;
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTaskAndReply(FROM_HERE, std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<WifiDirectServerSocket> server_socket_;
};

TEST_F(WifiDirectServerSocketTest, Close) {
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectServerSocket* socket) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(socket->Close().Ok());
      },
      socket()));
}

TEST_F(WifiDirectServerSocketTest, Close_MultipleCalls) {
  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectServerSocket* socket) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(socket->Close().Ok());
        EXPECT_FALSE(socket->Close().Ok());
      },
      socket()));
}

}  // namespace nearby::chrome

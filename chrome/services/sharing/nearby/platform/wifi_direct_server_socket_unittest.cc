// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_server_socket.h"

#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestIPv4Address[] = "127.0.0.1";
constexpr uint16_t kMinPort = 49152;  // Arbitrary port used for WifiLan.
constexpr uint16_t kMaxPort = 65535;

constexpr char kAcceptResultMetricName[] =
    "Nearby.Connections.WifiDirect.ServerSocket.Accept.Result";
constexpr char kAcceptErrorMetricName[] =
    "Nearby.Connections.WifiDirect.ServerSocket.Accept.Error";

}  // namespace

namespace nearby::chrome {

class WifiDirectServerSocketTest : public ::testing::Test {
 public:
  // ::testing::Test
  void SetUp() override {
    mojo::PendingRemote<::sharing::mojom::FirewallHole> firewall_hole;
    firewall_hole_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<ash::nearby::FakeFirewallHole>(),
        firewall_hole.InitWithNewPipeAndPassReceiver());

    auto tcp_server_socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());

    std::optional<net::IPAddress> address =
        net::IPAddress::FromIPLiteral(kTestIPv4Address);
    // Pick a random port for each test run, otherwise the `Listen` call below
    // has a chance to return ADDRESS_IN_USE(-147).
    port_ = static_cast<uint16_t>(kMinPort +
                                  base::RandGenerator(kMaxPort - kMinPort + 1));
    net::IPEndPoint end_point(*address, port_);
    EXPECT_EQ(
        tcp_server_socket->Listen(end_point,
                                  /*backlog=*/1, /*ipv6_only=*/std::nullopt),
        net::OK);

    // Create the subject under test, using the main thread task runner so that
    // the socket operations happen on the same thread they were created.
    server_socket_ = std::make_unique<WifiDirectServerSocket>(
        task_environment_.GetMainThreadTaskRunner(), mojo::PlatformHandle(),
        std::move(firewall_hole), std::move(tcp_server_socket));
  }

  void ConnectToSocket() {
    auto ip_endpoint = net::IPEndPoint(
        net::IPAddress::FromIPLiteral(kTestIPv4Address).value(), port_);
    client_socket_ = std::make_unique<net::TCPClientSocket>(
        net::AddressList(ip_endpoint), nullptr, nullptr, nullptr,
        net::NetLogSource());
    int result = client_socket_->Connect(base::BindOnce(
        &WifiDirectServerSocketTest::OnConnect, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING) {
      OnConnect(result);
    }
  }

  void OnConnect(int result) {
    EXPECT_EQ(result, net::OK);
    client_socket_->Disconnect();
  }

  WifiDirectServerSocket* socket() { return server_socket_.get(); }
  uint16_t port() { return port_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void RunOnTaskRunner(base::OnceClosure task) {
    base::RunLoop run_loop;
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTaskAndReply(FROM_HERE, std::move(task), run_loop.QuitClosure());
    run_loop.Run();
  }

  void RunDelayedOnTaskRunner(base::TimeDelta delay, base::OnceClosure task) {
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostDelayedTask(FROM_HERE, std::move(task), delay);
  }

  void RunDelayedOnIOThread(base::TimeDelta delay, base::OnceClosure task) {
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, std::move(task), delay);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  mojo::SelfOwnedReceiverRef<::sharing::mojom::FirewallHole> firewall_hole_;
  std::unique_ptr<WifiDirectServerSocket> server_socket_;
  uint16_t port_;
  std::unique_ptr<net::TCPClientSocket> client_socket_;
  base::HistogramTester histogram_tester_;
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

TEST_F(WifiDirectServerSocketTest, Close_WhileAccepting) {
  RunDelayedOnTaskRunner(
      base::Seconds(1),
      base::BindOnce(
          [](WifiDirectServerSocket* socket) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow;
            EXPECT_TRUE(socket->Close().Ok());
          },
          socket()));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectServerSocket* socket) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_FALSE(socket->Accept());
      },
      socket()));
}

TEST_F(WifiDirectServerSocketTest, GetIPAddress) {
  EXPECT_EQ(socket()->GetIPAddress(), kTestIPv4Address);
}

TEST_F(WifiDirectServerSocketTest, GetPort) {
  EXPECT_EQ(socket()->GetPort(), port());
}

TEST_F(WifiDirectServerSocketTest, Accept_ConnectionBeforeAccept) {
  histogram_tester().ExpectTotalCount(kAcceptResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kAcceptErrorMetricName, 0);

  // Connect to the socket before `Accept` is called to queue up a pending
  // connection.
  ConnectToSocket();

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectServerSocket* socket) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(socket->Accept());
      },
      socket()));

  histogram_tester().ExpectTotalCount(kAcceptResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kAcceptResultMetricName,
                                       /*bucket:success=*/1, 1);
  histogram_tester().ExpectTotalCount(kAcceptErrorMetricName, 0);
}

TEST_F(WifiDirectServerSocketTest, Accept_ConnectionAfterAccept) {
  histogram_tester().ExpectTotalCount(kAcceptResultMetricName, 0);
  histogram_tester().ExpectTotalCount(kAcceptErrorMetricName, 0);

  RunDelayedOnIOThread(
      base::Seconds(1),
      base::BindOnce(&WifiDirectServerSocketTest::ConnectToSocket,
                     base::Unretained(this)));

  RunOnTaskRunner(base::BindOnce(
      [](WifiDirectServerSocket* socket) {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        EXPECT_TRUE(socket->Accept());
      },
      socket()));

  histogram_tester().ExpectTotalCount(kAcceptResultMetricName, 1);
  histogram_tester().ExpectBucketCount(kAcceptResultMetricName,
                                       /*bucket:true=*/1, 1);
  histogram_tester().ExpectTotalCount(kAcceptErrorMetricName, 0);
}

}  // namespace nearby::chrome

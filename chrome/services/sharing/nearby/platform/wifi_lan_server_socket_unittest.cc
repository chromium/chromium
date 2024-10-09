// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_lan_server_socket.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_connected_socket.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_server_socket.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {

const char kExpectedLocalIpString[] = "\xC0\xA8\x56\x4B";
const int kExpectedLocalPort = 44444;
const net::IPEndPoint kLocalAddress(net::IPAddress(192, 168, 86, 75),
                                    kExpectedLocalPort);

const net::IPEndPoint kRemoteAddress(net::IPAddress(192, 168, 86, 62), 33333);

}  // namespace

class WifiLanServerSocketTest : public testing::Test {
 public:
  WifiLanServerSocketTest() = default;
  ~WifiLanServerSocketTest() override = default;
  WifiLanServerSocketTest(const WifiLanServerSocketTest&) = delete;
  WifiLanServerSocketTest& operator=(const WifiLanServerSocketTest&) = delete;

  void SetUp() override {
    auto fake_tcp_server_socket =
        std::make_unique<ash::nearby::FakeTcpServerSocket>();
    fake_tcp_server_socket_ = fake_tcp_server_socket.get();
    mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket;
    tcp_server_socket_self_owned_receiver_ref_ = mojo::MakeSelfOwnedReceiver(
        std::move(fake_tcp_server_socket),
        tcp_server_socket.InitWithNewPipeAndPassReceiver());

    mojo::PendingRemote<::sharing::mojom::FirewallHole> firewall_hole;
    firewall_hole_self_owned_receiver_ref_ = mojo::MakeSelfOwnedReceiver(
        std::make_unique<ash::nearby::FakeFirewallHole>(),
        firewall_hole.InitWithNewPipeAndPassReceiver());

    wifi_lan_server_socket_ = std::make_unique<WifiLanServerSocket>(
        WifiLanServerSocket::ServerSocketParameters(
            kLocalAddress, std::move(tcp_server_socket),
            std::move(firewall_hole)));
  }

  void TearDown() override { wifi_lan_server_socket_.reset(); }

  // Calls Accept() from |num_threads|, which will each block until failure or
  // until our fake TCP server socket establishes a connection with a remote
  // device. This method returns when
  // |expected_num_accept_calls_sent_to_tcp_socket| TCPServerSocket::Accept()
  // calls are queued up. When the WifiLanServerSocket::Accept() calls finish on
  // all threads, |on_accept_calls_finished| is invoked.
  void CallAcceptFromThreads(
      size_t num_threads,
      size_t expected_num_accept_calls_sent_to_tcp_socket,
      bool expected_success,
      base::OnceClosure on_accept_calls_finished) {
    // The run loop quits when the TCP server socket receives all of the
    // expected Accept() calls.
    base::RunLoop run_loop;
    fake_tcp_server_socket_->SetAcceptCallExpectations(
        expected_num_accept_calls_sent_to_tcp_socket,
        /*on_all_accept_calls_queued=*/run_loop.QuitClosure());

    on_accept_calls_finished_ = std::move(on_accept_calls_finished);
    num_running_accept_calls_ = num_threads;
    for (size_t thread = 0; thread < num_threads; ++thread) {
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&WifiLanServerSocketTest::CallAccept,
                                    base::Unretained(this), expected_success));
    }
    run_loop.Run();
  }

 protected:
  void CallAccept(bool expected_success) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow;
    std::unique_ptr<api::WifiLanSocket> connected_socket =
        wifi_lan_server_socket_->Accept();

    ASSERT_EQ(expected_success, connected_socket != nullptr);
    if (--num_running_accept_calls_ == 0) {
      std::move(on_accept_calls_finished_).Run();
    }
  }

  base::test::TaskEnvironment task_environment_;
  size_t num_running_accept_calls_ = 0;
  base::OnceClosure on_accept_calls_finished_;
  raw_ptr<ash::nearby::FakeTcpServerSocket, DanglingUntriaged>
      fake_tcp_server_socket_;
  mojo::SelfOwnedReceiverRef<network::mojom::TCPServerSocket>
      tcp_server_socket_self_owned_receiver_ref_;
  mojo::SelfOwnedReceiverRef<::sharing::mojom::FirewallHole>
      firewall_hole_self_owned_receiver_ref_;
  std::unique_ptr<WifiLanServerSocket> wifi_lan_server_socket_;
};

TEST_F(WifiLanServerSocketTest, GetAddressAndPort) {
  EXPECT_EQ(kExpectedLocalIpString, wifi_lan_server_socket_->GetIPAddress());
  EXPECT_EQ(kExpectedLocalPort, wifi_lan_server_socket_->GetPort());
}

TEST_F(WifiLanServerSocketTest, Accept_Success) {
  base::RunLoop run_loop;
  CallAcceptFromThreads(
      /*num_threads=*/1u,
      /*expected_num_accept_calls_sent_to_tcp_socket=*/1u,
      /*expected_success=*/true,
      /*on_accept_calls_finished=*/run_loop.QuitClosure());
  fake_tcp_server_socket_->FinishNextAccept(net::OK, kRemoteAddress);
  run_loop.Run();
}

TEST_F(WifiLanServerSocketTest, Accept_Success_ConcurrentCalls) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallAcceptFromThreads(
      kNumThreads,
      /*expected_num_accept_calls_sent_to_tcp_socket=*/kNumThreads,
      /*expected_success=*/true,
      /*on_accept_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_tcp_server_socket_->FinishNextAccept(net::OK, kRemoteAddress);
  }
  run_loop.Run();
}

TEST_F(WifiLanServerSocketTest, Accept_Failure) {
  base::RunLoop run_loop;
  CallAcceptFromThreads(
      /*num_threads=*/1u,
      /*expected_num_accept_calls_sent_to_tcp_socket=*/1u,
      /*expected_success=*/false,
      /*on_accept_calls_finished=*/run_loop.QuitClosure());
  fake_tcp_server_socket_->FinishNextAccept(net::ERR_FAILED,
                                            /*remote_addr=*/std::nullopt);
  run_loop.Run();
}

TEST_F(WifiLanServerSocketTest, Accept_Failure_ConcurrentCalls) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallAcceptFromThreads(
      kNumThreads,
      /*expected_num_accept_calls_sent_to_tcp_socket=*/kNumThreads,
      /*expected_success=*/false,
      /*on_accept_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_tcp_server_socket_->FinishNextAccept(net::ERR_FAILED,
                                              /*remote_addr=*/std::nullopt);
  }
  run_loop.Run();
}

TEST_F(WifiLanServerSocketTest, Accept_AfterClose) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  wifi_lan_server_socket_->Close();

  // Because the WifiLanServerSocket is already closed, we expect the logic to
  // short-ciruit and not invoke TCPServerSocket::Accept().
  CallAcceptFromThreads(kNumThreads,
                        /*expected_num_accept_calls_sent_to_tcp_socket=*/0u,
                        /*expected_success=*/false,
                        /*on_accept_calls_finished=*/run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(WifiLanServerSocketTest, Close_WhileWaitingForAccept) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallAcceptFromThreads(
      kNumThreads,
      /*expected_num_accept_calls_sent_to_tcp_socket=*/kNumThreads,
      /*expected_success=*/false,
      /*on_accept_calls_finished=*/run_loop.QuitClosure());

  // Close cancels all pending Accept() calls.
  wifi_lan_server_socket_->Close();
  run_loop.Run();
}

TEST_F(WifiLanServerSocketTest, Close_CalledFromMultipleThreads) {
  base::RunLoop run_loop;
  const size_t kNumThreads = 3;

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
          EXPECT_EQ(Exception::kSuccess,
                    wifi_lan_server_socket_->Close().value);
        }),
        quit_callback);
  }
  run_loop.Run();
}

TEST_F(WifiLanServerSocketTest, Destroy_WhileWaitingForAccept) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallAcceptFromThreads(
      kNumThreads,
      /*expected_num_accept_calls_sent_to_tcp_socket=*/kNumThreads,
      /*expected_success=*/false,
      /*on_accept_calls_finished=*/run_loop.QuitClosure());

  // The WifiLanServerSocket calls Close() during destruction, which cancels the
  // pending Accept() calls.
  wifi_lan_server_socket_.reset();
  run_loop.Run();
}

TEST_F(WifiLanServerSocketTest,
       Disconnect_WhileWaitingForAccept_TcpServerSocket) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallAcceptFromThreads(
      kNumThreads,
      /*expected_num_accept_calls_sent_to_tcp_socket=*/kNumThreads,
      /*expected_success=*/false,
      /*on_accept_calls_finished=*/run_loop.QuitClosure());

  // Destroying the TCPServerSocket receiver will trigger the remote's
  // disconnect handler, which will close the WifiLanServerSocket.
  tcp_server_socket_self_owned_receiver_ref_->Close();
  run_loop.Run();
}

TEST_F(WifiLanServerSocketTest, Disconnect_WhileWaitingForAccept_FirewallHole) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallAcceptFromThreads(
      kNumThreads,
      /*expected_num_accept_calls_sent_to_tcp_socket=*/kNumThreads,
      /*expected_success=*/false,
      /*on_accept_calls_finished=*/run_loop.QuitClosure());

  // Destroying the FirewallHole receiver will trigger the remote's
  // disconnect handler, which will close the WifiLanServerSocket.
  firewall_hole_self_owned_receiver_ref_->Close();
  run_loop.Run();
}

}  // namespace nearby::chrome

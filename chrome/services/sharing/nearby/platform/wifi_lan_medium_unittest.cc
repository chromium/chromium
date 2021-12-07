// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_lan_medium.h"

#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/fake_network_context.h"
#include "chrome/services/sharing/nearby/platform/wifi_lan_server_socket.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace location {
namespace nearby {
namespace chrome {

namespace {

const char kLocalIpString[] = "\xC0\xA8\x56\x4B";
const int kLocalPort = 44444;
const net::IPEndPoint kLocalAddress(net::IPAddress(192, 168, 86, 75),
                                    kLocalPort);

const char kRemoteIpString[] = "\xC0\xA8\x56\x3E";
const int kRemotePort = 33333;

}  // namespace

class WifiLanMediumTest : public ::testing::Test {
 public:
  WifiLanMediumTest() = default;
  ~WifiLanMediumTest() override = default;
  WifiLanMediumTest(const WifiLanMediumTest&) = delete;
  WifiLanMediumTest& operator=(const WifiLanMediumTest&) = delete;

  void SetUp() override {
    auto fake_network_context = std::make_unique<FakeNetworkContext>(
        /*default_local_addr=*/kLocalAddress);
    fake_network_context_ = fake_network_context.get();
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_network_context),
        network_context_shared_remote_.BindNewPipeAndPassReceiver());

    wifi_lan_medium_ =
        std::make_unique<WifiLanMedium>(network_context_shared_remote_);
  }

  void TearDown() override { wifi_lan_medium_.reset(); }

  // Calls ConnectToService()/ListenForService() from |num_threads|, which will
  // each block until failure or the TCP connected/server socket is created.
  // This method returns when |expected_num_calls_sent_to_network_context|
  // NetworkContext::CreateTCPConnectedSocket()/CreateTCPServerSocket() calls
  // are queued up. When the ConnectToService()/ListenForService() calls finish
  // on all threads, |on_finished| is invoked.
  void CallConnectToServiceFromThreads(
      size_t num_threads,
      size_t expected_num_calls_sent_to_network_context,
      bool expected_success,
      base::OnceClosure on_connect_calls_finished) {
    // The run loop quits when NetworkContext receives all of the expected
    // CreateTCPConnectedSocket() calls.
    base::RunLoop run_loop;
    fake_network_context_->SetCreateConnectedSocketCallExpectations(
        expected_num_calls_sent_to_network_context,
        /*on_all_create_connected_socket_calls_queued=*/run_loop.QuitClosure());

    on_connect_calls_finished_ = std::move(on_connect_calls_finished);
    num_running_connect_calls_ = num_threads;
    for (size_t thread = 0; thread < num_threads; ++thread) {
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&WifiLanMediumTest::CallConnect,
                                    base::Unretained(this), expected_success));
    }
    run_loop.Run();
  }

  void CallListenForServiceFromThreads(
      size_t num_threads,
      size_t expected_num_calls_sent_to_network_context,
      bool expected_success,
      base::OnceClosure on_listen_calls_finished) {
    // The run loop quits when NetworkContext receives all of the expected
    // CreateTCPServerSocket() calls.
    base::RunLoop run_loop;
    fake_network_context_->SetCreateServerSocketCallExpectations(
        expected_num_calls_sent_to_network_context,
        /*on_all_create_server_socket_calls_queued=*/run_loop.QuitClosure());

    on_listen_calls_finished_ = std::move(on_listen_calls_finished);
    num_running_listen_calls_ = num_threads;
    for (size_t thread = 0; thread < num_threads; ++thread) {
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
          ->PostTask(FROM_HERE,
                     base::BindOnce(&WifiLanMediumTest::CallListen,
                                    base::Unretained(this), expected_success));
    }
    run_loop.Run();
  }

 protected:
  void CallConnect(bool expected_success) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow;
    std::unique_ptr<api::WifiLanSocket> connected_socket =
        wifi_lan_medium_->ConnectToService(kRemoteIpString, kRemotePort,
                                           /*cancellation_flag=*/nullptr);

    ASSERT_EQ(expected_success, connected_socket != nullptr);
    if (--num_running_connect_calls_ == 0) {
      std::move(on_connect_calls_finished_).Run();
    }
  }

  void CallListen(bool expected_success) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow;
    std::unique_ptr<api::WifiLanServerSocket> server_socket =
        wifi_lan_medium_->ListenForService(/*port=*/kLocalPort);

    ASSERT_EQ(expected_success, server_socket != nullptr);
    if (expected_success) {
      // Verify that the server socket has the expected local IP:port.
      EXPECT_EQ(kLocalIpString, server_socket->GetIPAddress());
      EXPECT_EQ(kLocalPort, server_socket->GetPort());
    }

    if (--num_running_listen_calls_ == 0) {
      std::move(on_listen_calls_finished_).Run();
    }
  }

  base::test::TaskEnvironment task_environment_;
  size_t num_running_connect_calls_ = 0;
  size_t num_running_listen_calls_ = 0;
  base::OnceClosure on_connect_calls_finished_;
  base::OnceClosure on_listen_calls_finished_;
  FakeNetworkContext* fake_network_context_;
  mojo::SharedRemote<network::mojom::NetworkContext>
      network_context_shared_remote_;
  std::unique_ptr<WifiLanMedium> wifi_lan_medium_;
};

/*============================================================================*/
// Begin: ConnectToService()
/*============================================================================*/
TEST_F(WifiLanMediumTest, Connect_Success) {
  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_network_context=*/1u,
      /*expected_success=*/true,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());
  fake_network_context_->FinishNextCreateConnectedSocket(net::OK);
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Connect_Success_ConcurrentCalls) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_network_context=*/kNumThreads,
      /*expected_success=*/true,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_network_context_->FinishNextCreateConnectedSocket(net::OK);
  }
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Connect_Failure) {
  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_network_context=*/1u,
      /*expected_success=*/false,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());
  fake_network_context_->FinishNextCreateConnectedSocket(net::ERR_FAILED);
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Connect_Failure_ConcurrentCalls) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_network_context=*/kNumThreads,
      /*expected_success=*/false,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_network_context_->FinishNextCreateConnectedSocket(net::ERR_FAILED);
  }
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Connect_DestroyWhileWaiting) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallConnectToServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_network_context=*/kNumThreads,
      /*expected_success=*/false,
      /*on_connect_calls_finished=*/run_loop.QuitClosure());

  // The WifiLanMedium cancels all pending calls before destruction.
  wifi_lan_medium_.reset();
  run_loop.Run();
}
/*============================================================================*/
// End: ConnectToService()
/*============================================================================*/

/*============================================================================*/
// Begin: ListenForService()
/*============================================================================*/
TEST_F(WifiLanMediumTest, Listen_Success) {
  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_network_context=*/1u,
      /*expected_success=*/true,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  fake_network_context_->FinishNextCreateServerSocket(net::OK);
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Success_ConcurrentCalls) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_network_context=*/kNumThreads,
      /*expected_success=*/true,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_network_context_->FinishNextCreateServerSocket(net::OK);
  }
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Failure) {
  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      /*num_threads=*/1u,
      /*expected_num_calls_sent_to_network_context=*/1u,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  fake_network_context_->FinishNextCreateServerSocket(net::ERR_FAILED);
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_Failure_ConcurrentCalls) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_network_context=*/kNumThreads,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());
  for (size_t thread = 0; thread < kNumThreads; ++thread) {
    fake_network_context_->FinishNextCreateServerSocket(net::ERR_FAILED);
  }
  run_loop.Run();
}

TEST_F(WifiLanMediumTest, Listen_DestroyWhileWaiting) {
  const size_t kNumThreads = 3;
  base::RunLoop run_loop;
  CallListenForServiceFromThreads(
      kNumThreads,
      /*expected_num_calls_sent_to_network_context=*/kNumThreads,
      /*expected_success=*/false,
      /*on_listen_calls_finished=*/run_loop.QuitClosure());

  // The WifiLanMedium cancels all pending calls before destruction.
  wifi_lan_medium_.reset();
  run_loop.Run();
}
/*============================================================================*/
// End: ListenForService()
/*============================================================================*/

}  // namespace chrome
}  // namespace nearby
}  // namespace location

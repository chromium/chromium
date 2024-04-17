// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/tls_connection_factory.h"

#include <iostream>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/openscreen_platform/network_context.h"
#include "components/openscreen_platform/tls_client_connection.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

using openscreen::Error;
using openscreen::TlsConnection;
using openscreen::TlsConnectOptions;

namespace openscreen_platform {

namespace {

const openscreen::IPEndpoint kValidOpenscreenEndpoint{
    openscreen::IPAddress{192, 168, 0, 1}, 80};

class MockTlsConnectionFactoryClient
    : public openscreen::TlsConnectionFactory::Client {
 public:
  MOCK_METHOD(void,
              OnAccepted,
              (openscreen::TlsConnectionFactory*,
               std::vector<uint8_t>,
               std::unique_ptr<TlsConnection>),
              (override));
  MOCK_METHOD(void,
              OnConnected,
              (openscreen::TlsConnectionFactory*,
               std::vector<uint8_t>,
               std::unique_ptr<TlsConnection>),
              (override));
  MOCK_METHOD(void,
              OnConnectionFailed,
              (openscreen::TlsConnectionFactory*,
               const openscreen::IPEndpoint&),
              (override));
  MOCK_METHOD(void,
              OnError,
              (openscreen::TlsConnectionFactory*, const Error&),
              (override));
};

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  void CreateTCPConnectedSocket(
      const std::optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override {
    ++times_called_;
    callback_ = std::move(callback);
  }

  int times_called() { return times_called_; }

  void ExecuteCreateCallback(int32_t net_result) {
    std::move(callback_).Run(net_result, std::nullopt, std::nullopt,
                             mojo::ScopedDataPipeConsumerHandle{},
                             mojo::ScopedDataPipeProducerHandle{});
  }

 private:
  CreateTCPConnectedSocketCallback callback_;
  int times_called_ = 0;
};

}  // namespace

class TlsConnectionFactoryTest : public ::testing::Test {
 public:
  void SetUp() override {
    mock_network_context = std::make_unique<FakeNetworkContext>();
    SetNetworkContextGetter(base::BindRepeating(
        &TlsConnectionFactoryTest::GetNetworkContext, base::Unretained(this)));
  }

  void TearDown() override {
    SetNetworkContextGetter(network::NetworkContextGetter());
  }

 protected:
  network::mojom::NetworkContext* GetNetworkContext() {
    return mock_network_context.get();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeNetworkContext> mock_network_context;
};

TEST_F(TlsConnectionFactoryTest, CallsNetworkContextCreateMethod) {
  StrictMock<MockTlsConnectionFactoryClient> mock_client;
  TlsConnectionFactory factory(mock_client);

  factory.Connect(kValidOpenscreenEndpoint, TlsConnectOptions{});

  mock_network_context->ExecuteCreateCallback(net::OK);
  EXPECT_EQ(1, mock_network_context->times_called());
}

TEST_F(TlsConnectionFactoryTest,
       CallsOnConnectionFailedWhenNetworkContextReportsError) {
  StrictMock<MockTlsConnectionFactoryClient> mock_client;
  TlsConnectionFactory factory(mock_client);
  EXPECT_CALL(mock_client,
              OnConnectionFailed(&factory, kValidOpenscreenEndpoint));

  factory.Connect(kValidOpenscreenEndpoint, TlsConnectOptions{});

  mock_network_context->ExecuteCreateCallback(net::ERR_FAILED);
  EXPECT_EQ(1, mock_network_context->times_called());
  base::RunLoop().RunUntilIdle();
}

}  // namespace openscreen_platform

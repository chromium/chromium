// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/websocket_client.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "url/gurl.h"

namespace private_ai {

namespace {

class MockNetworkContext : public network::TestNetworkContext {
 public:
  void CreateWebSocket(
      const GURL& url,
      const std::vector<std::string>& requested_protocols,
      net::StorageAccessApiStatus storage_access_api_status,
      const net::IsolationInfo& isolation_info,
      std::vector<network::mojom::HttpHeaderPtr> additional_headers,
      const network::OriginatingProcessId& process_id,
      const url::Origin& origin,
      network::mojom::ClientSecurityStatePtr client_security_state,
      uint32_t options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
          auth_handler,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client,
      const std::optional<base::UnguessableToken>& throttling_profile_id,
      const base::UnguessableToken& network_restrictions_id,
      network::mojom::IPAddressSpace target_address_space) override {
    create_called_ = true;
    pending_handshake_client_ = std::move(handshake_client);
    additional_headers_ = std::move(additional_headers);
  }

  std::optional<std::string> GetHeader(std::string_view name) const {
    for (const auto& header : additional_headers_) {
      if (header->name == name) {
        return header->value;
      }
    }
    return std::nullopt;
  }

  bool create_called_ = false;
  mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
      pending_handshake_client_;
  std::vector<network::mojom::HttpHeaderPtr> additional_headers_;
};

struct TestConnection {
  mojo::Remote<network::mojom::WebSocketClient> client_remote;
  mojo::Remote<network::mojom::WebSocket> websocket_remote;
  mojo::ScopedDataPipeProducerHandle server_to_client_producer;
  mojo::ScopedDataPipeConsumerHandle client_to_server_consumer;
};

class WebSocketClientTest : public ::testing::Test {
 protected:
  WebSocketClientTest()
      : client_(GURL("wss://example.com/websocket"),
                &network_context_,
                &logger_) {
    client_.SetResponseCallback(future_.GetRepeatingCallback());
  }

  TestConnection EstablishConnection() {
    client_.Send(oak::session::v1::SessionRequest());
    EXPECT_TRUE(network_context_.create_called_);
    EXPECT_TRUE(network_context_.pending_handshake_client_.is_valid());

    mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client(
        std::move(network_context_.pending_handshake_client_));

    TestConnection conn;
    auto client_receiver = conn.client_remote.BindNewPipeAndPassReceiver();
    auto websocket_receiver =
        conn.websocket_remote.BindNewPipeAndPassReceiver();

    mojo::ScopedDataPipeConsumerHandle readable_consumer;
    CHECK_EQ(mojo::CreateDataPipe(nullptr, conn.server_to_client_producer,
                                  readable_consumer),
             MOJO_RESULT_OK);

    mojo::ScopedDataPipeProducerHandle writable_producer;
    CHECK_EQ(mojo::CreateDataPipe(nullptr, writable_producer,
                                  conn.client_to_server_consumer),
             MOJO_RESULT_OK);

    handshake_client->OnConnectionEstablished(
        conn.websocket_remote.Unbind(), std::move(client_receiver),
        network::mojom::WebSocketHandshakeResponse::New(),
        std::move(readable_consumer), std::move(writable_producer));
    return conn;
  }

  base::test::TaskEnvironment task_environment_;
  PrivateAiLogger logger_;
  MockNetworkContext network_context_;
  base::test::TestFuture<base::expected<oak::session::v1::SessionResponse,
                                        Transport::TransportError>>
      future_;
  WebSocketClient client_;
};

TEST_F(WebSocketClientTest, ConnectionFailure) {
  client_.Send(oak::session::v1::SessionRequest());

  EXPECT_TRUE(network_context_.create_called_);
  ASSERT_TRUE(network_context_.pending_handshake_client_.is_valid());

  mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client(
      std::move(network_context_.pending_handshake_client_));

  handshake_client->OnFailure("Connection failed", net::ERR_FAILED, 500);

  const auto& result = future_.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Transport::TransportError::kError);
}

TEST_F(WebSocketClientTest, ChannelDropped) {
  base::HistogramTester histogram_tester;
  auto conn = EstablishConnection();

  conn.client_remote->OnDropChannel(true, 1000, "Normal closure");

  const auto& result = future_.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Transport::TransportError::kSocketClosed);

  histogram_tester.ExpectTotalCount(
      "PrivateAi.Client.WebSocketSessionDuration.ClosedByServer", 1);
  histogram_tester.ExpectUniqueSample("PrivateAi.Client.WebSocketCloseCode",
                                      1000, 1);
}

TEST_F(WebSocketClientTest, SuccessfulResponse) {
  auto conn = EstablishConnection();

  oak::session::v1::SessionResponse session_response;
  session_response.mutable_encrypted_message()->set_ciphertext("hello world");
  std::string full_data;
  ASSERT_TRUE(session_response.SerializeToString(&full_data));

  size_t bytes_written = 0;
  ASSERT_EQ(conn.server_to_client_producer->WriteData(
                base::as_bytes(base::span(full_data)),
                MOJO_WRITE_DATA_FLAG_NONE, bytes_written),
            MOJO_RESULT_OK);
  EXPECT_EQ(bytes_written, full_data.size());

  conn.client_remote->OnDataFrame(
      true, network::mojom::WebSocketMessageType::BINARY, full_data.size());

  const auto& result = future_.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().encrypted_message().ciphertext(), "hello world");
}

TEST_F(WebSocketClientTest, NonProdVerificationKeyVariantHeader) {
  GURL url("wss://dev-private-ai.example.com/websocket");
  MockNetworkContext network_context;

  WebSocketClient client(url, &network_context, &logger_);
  client.SetResponseCallback(base::DoNothing());
  client.Send(oak::session::v1::SessionRequest());

  EXPECT_TRUE(network_context.create_called_);
  EXPECT_EQ(network_context.GetHeader("X-WebChannel-Content-Type"),
            "application/x-protobuf");
  EXPECT_EQ(network_context.GetHeader("X-Client-Verification-Key-Variant"),
            "nonprod");
}

TEST_F(WebSocketClientTest, ProdVerificationKeyVariantHeader) {
  GURL url("wss://example.com/websocket");
  MockNetworkContext network_context;

  WebSocketClient client(url, &network_context, &logger_);
  client.SetResponseCallback(base::DoNothing());
  client.Send(oak::session::v1::SessionRequest());

  EXPECT_TRUE(network_context.create_called_);
  EXPECT_EQ(network_context.GetHeader("X-WebChannel-Content-Type"),
            "application/x-protobuf");
  EXPECT_FALSE(network_context.GetHeader("X-Client-Verification-Key-Variant")
                   .has_value());
}
}  // namespace

}  // namespace private_ai

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/websocket_client.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "services/network/public/mojom/network_context.mojom.h"
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
      const std::optional<base::UnguessableToken>& network_restrictions_id)
      override {
    create_called_ = true;
    pending_handshake_client_ = std::move(handshake_client);

    for (const auto& header : additional_headers) {
      if (header->name == "x-client-data" || header->name == "X-Client-Data") {
        has_x_client_data_ = true;
      }
      if (header->name == "X-WebChannel-Content-Type") {
        has_content_type_ = true;
        EXPECT_EQ(header->value, "application/x-protobuf");
      }
    }
  }

  bool create_called_ = false;
  bool has_x_client_data_ = false;
  bool has_content_type_ = false;
  mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
      pending_handshake_client_;
};

class WebSocketClientTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  PrivateAiLogger logger_;
};

TEST_F(WebSocketClientTest, ConnectionFailure) {
  GURL url("wss://example.com/websocket");
  MockNetworkContext network_context;

  WebSocketClient client(url, &network_context, &logger_);

  base::test::TestFuture<base::expected<oak::session::v1::SessionResponse,
                                        Transport::TransportError>>
      future;
  client.SetResponseCallback(future.GetRepeatingCallback());

  client.Send(oak::session::v1::SessionRequest());

  EXPECT_TRUE(network_context.create_called_);
  ASSERT_TRUE(network_context.pending_handshake_client_.is_valid());

  mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client(
      std::move(network_context.pending_handshake_client_));

  handshake_client->OnFailure("Connection failed", net::ERR_FAILED, 500);

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Transport::TransportError::kError);
}

TEST_F(WebSocketClientTest, ChannelDropped) {
  GURL url("wss://example.com/websocket");
  MockNetworkContext network_context;

  WebSocketClient client(url, &network_context, &logger_);

  base::test::TestFuture<base::expected<oak::session::v1::SessionResponse,
                                        Transport::TransportError>>
      future;
  client.SetResponseCallback(future.GetRepeatingCallback());

  client.Send(oak::session::v1::SessionRequest());

  EXPECT_TRUE(network_context.create_called_);
  ASSERT_TRUE(network_context.pending_handshake_client_.is_valid());

  mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client(
      std::move(network_context.pending_handshake_client_));

  mojo::Remote<network::mojom::WebSocketClient> client_remote;
  auto client_receiver = client_remote.BindNewPipeAndPassReceiver();

  mojo::Remote<network::mojom::WebSocket> websocket_remote;
  auto websocket_receiver = websocket_remote.BindNewPipeAndPassReceiver();

  mojo::ScopedDataPipeProducerHandle producer1;
  mojo::ScopedDataPipeConsumerHandle consumer1;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer1, consumer1),
            MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle producer2;
  mojo::ScopedDataPipeConsumerHandle consumer2;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer2, consumer2),
            MOJO_RESULT_OK);

  handshake_client->OnConnectionEstablished(
      websocket_remote.Unbind(), std::move(client_receiver),
      network::mojom::WebSocketHandshakeResponse::New(), std::move(consumer1),
      std::move(producer2));

  client_remote->OnDropChannel(true, 1000, "Normal closure");

  const auto& result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Transport::TransportError::kSocketClosed);
}

TEST_F(WebSocketClientTest, MessageFragmentation) {
  GURL url("wss://example.com/websocket");
  MockNetworkContext network_context;

  WebSocketClient client(url, &network_context, &logger_);

  base::test::TestFuture<base::expected<oak::session::v1::SessionResponse,
                                        Transport::TransportError>>
      future;
  client.SetResponseCallback(future.GetRepeatingCallback());

  client.Send(oak::session::v1::SessionRequest());

  EXPECT_TRUE(network_context.create_called_);
  ASSERT_TRUE(network_context.pending_handshake_client_.is_valid());

  mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client(
      std::move(network_context.pending_handshake_client_));

  mojo::Remote<network::mojom::WebSocketClient> client_remote;
  auto client_receiver = client_remote.BindNewPipeAndPassReceiver();

  mojo::Remote<network::mojom::WebSocket> websocket_remote;
  auto websocket_receiver = websocket_remote.BindNewPipeAndPassReceiver();

  mojo::ScopedDataPipeProducerHandle producer1;
  mojo::ScopedDataPipeConsumerHandle consumer1;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer1, consumer1),
            MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle producer2;
  mojo::ScopedDataPipeConsumerHandle consumer2;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer2, consumer2),
            MOJO_RESULT_OK);

  handshake_client->OnConnectionEstablished(
      websocket_remote.Unbind(), std::move(client_receiver),
      network::mojom::WebSocketHandshakeResponse::New(), std::move(consumer1),
      std::move(producer2));

  // Write all data to the pipe first to avoid blocking.
  oak::session::v1::SessionResponse session_response;
  session_response.mutable_encrypted_message()->set_ciphertext("hello world");
  std::string full_data;
  ASSERT_TRUE(session_response.SerializeToString(&full_data));

  size_t bytes_written = 0;
  ASSERT_EQ(producer1->WriteData(base::as_bytes(base::span(full_data)),
                                 MOJO_WRITE_DATA_FLAG_NONE, bytes_written),
            MOJO_RESULT_OK);
  EXPECT_EQ(bytes_written, full_data.size());

  // Simulate first frame (not finished)
  size_t half_size = full_data.size() / 2;
  client_remote->OnDataFrame(
      false, network::mojom::WebSocketMessageType::BINARY, half_size);

  // Simulate second frame (finished)
  client_remote->OnDataFrame(true,
                             network::mojom::WebSocketMessageType::CONTINUATION,
                             full_data.size() - half_size);

  const auto& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().encrypted_message().ciphertext(), "hello world");
}

TEST_F(WebSocketClientTest, NoXClientDataHeader) {
  GURL url("wss://example.com/websocket");
  MockNetworkContext network_context;

  WebSocketClient client(url, &network_context, &logger_);
  client.SetResponseCallback(base::DoNothing());
  client.Send(oak::session::v1::SessionRequest());

  EXPECT_TRUE(network_context.create_called_);
  EXPECT_TRUE(network_context.has_content_type_);
  // X-Client-Data should not be sent with the HTTP handshake. It may only be
  // sent to the enclave.
  EXPECT_FALSE(network_context.has_x_client_data_);
}

}  // namespace

}  // namespace private_ai

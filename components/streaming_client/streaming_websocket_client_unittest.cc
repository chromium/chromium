// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/streaming_client/streaming_websocket_client.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace streaming_client {

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
    create_called = true;
    pending_handshake_client = std::move(handshake_client);
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

  bool create_called = false;
  mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
      pending_handshake_client;
  std::vector<network::mojom::HttpHeaderPtr> additional_headers_;
};

class FakeWebSocket : public network::mojom::WebSocket {
 public:
  void SendMessage(network::mojom::WebSocketMessageType type,
                   uint64_t data_length) override {
    sent_messages.push_back({type, data_length});
  }

  void StartReceiving() override { is_receiving = true; }

  void StartClosingHandshake(uint16_t code,
                             const std::string& reason) override {}

  struct SentMessage {
    network::mojom::WebSocketMessageType type;
    uint64_t data_length;
  };

  bool is_receiving = false;
  std::vector<SentMessage> sent_messages;
};

class MockDelegate : public StreamingWebSocketClient::Delegate {
 public:
  void OnMessage(std::vector<uint8_t> message) override {
    messages.push_back(std::move(message));
    NotifyEvent();
  }

  void OnConnectionError(const std::string& message,
                         int net_error,
                         int response_code) override {
    connection_error = {message, net_error, response_code};
    NotifyEvent();
  }

  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason,
                     std::optional<base::TimeDelta> elapsed) override {
    drop_channel = {was_clean, code, reason, elapsed};
    NotifyEvent();
  }

  void OnError(const std::string& message) override {
    error_message = message;
    NotifyEvent();
  }

  void OnClose() override {
    closed = true;
    NotifyEvent();
  }

  std::vector<network::mojom::HttpHeaderPtr> GetAdditionalHeaders() override {
    std::vector<network::mojom::HttpHeaderPtr> headers;
    for (const auto& header : additional_headers) {
      headers.push_back(header.Clone());
    }
    return headers;
  }

  struct ConnectionError {
    std::string message;
    int net_error;
    int response_code;
  };

  struct DropChannel {
    bool was_clean;
    uint16_t code;
    std::string reason;
    std::optional<base::TimeDelta> elapsed;
  };

  void NotifyEvent() {
    if (on_event_callback) {
      on_event_callback.Run();
    }
  }

  std::vector<std::vector<uint8_t>> messages;
  std::optional<ConnectionError> connection_error;
  std::optional<DropChannel> drop_channel;
  std::optional<std::string> error_message;
  bool closed = false;
  std::vector<network::mojom::HttpHeaderPtr> additional_headers;
  base::RepeatingClosure on_event_callback;
};

struct TestConnection {
  mojo::Remote<network::mojom::WebSocketClient> client_remote;
  std::unique_ptr<FakeWebSocket> fake_websocket =
      std::make_unique<FakeWebSocket>();
  std::unique_ptr<mojo::Receiver<network::mojom::WebSocket>>
      websocket_receiver =
          std::make_unique<mojo::Receiver<network::mojom::WebSocket>>(
              fake_websocket.get());
  mojo::ScopedDataPipeProducerHandle server_to_client_producer;
  mojo::ScopedDataPipeConsumerHandle client_to_server_consumer;
};

class StreamingWebSocketClientTest : public ::testing::Test {
 protected:
  StreamingWebSocketClientTest()
      : client_(GURL("wss://example.com/websocket"),
                &network_context_,
                TRAFFIC_ANNOTATION_FOR_TESTS,
                &delegate_) {}

  TestConnection CompleteHandshake() {
    EXPECT_TRUE(network_context_.create_called);
    EXPECT_TRUE(network_context_.pending_handshake_client.is_valid());

    mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client(
        std::move(network_context_.pending_handshake_client));

    TestConnection conn;
    auto client_receiver = conn.client_remote.BindNewPipeAndPassReceiver();

    mojo::ScopedDataPipeConsumerHandle readable_consumer;
    CHECK_EQ(mojo::CreateDataPipe(nullptr, conn.server_to_client_producer,
                                  readable_consumer),
             MOJO_RESULT_OK);

    mojo::ScopedDataPipeProducerHandle writable_producer;
    CHECK_EQ(mojo::CreateDataPipe(nullptr, writable_producer,
                                  conn.client_to_server_consumer),
             MOJO_RESULT_OK);

    handshake_client->OnConnectionEstablished(
        conn.websocket_receiver->BindNewPipeAndPassRemote(),
        std::move(client_receiver),
        network::mojom::WebSocketHandshakeResponse::New(),
        std::move(readable_consumer), std::move(writable_producer));
    return conn;
  }

  TestConnection EstablishConnection(std::vector<uint8_t> initial_data = {1, 2,
                                                                          3}) {
    client_.Send(std::move(initial_data));
    return CompleteHandshake();
  }

  base::test::TaskEnvironment task_environment_;
  MockNetworkContext network_context_;
  MockDelegate delegate_;
  StreamingWebSocketClient client_;
};

TEST_F(StreamingWebSocketClientTest, ConnectionFailure) {
  base::test::TestFuture<void> future;
  delegate_.on_event_callback = future.GetRepeatingCallback();

  client_.Send(std::vector<uint8_t>{1, 2, 3});

  EXPECT_TRUE(network_context_.create_called);
  ASSERT_TRUE(network_context_.pending_handshake_client.is_valid());

  mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client(
      std::move(network_context_.pending_handshake_client));

  handshake_client->OnFailure("Connection failed", net::ERR_FAILED, 500);

  EXPECT_TRUE(future.Wait());
  ASSERT_TRUE(delegate_.connection_error.has_value());
  EXPECT_EQ(delegate_.connection_error->message, "Connection failed");
  EXPECT_EQ(delegate_.connection_error->net_error, net::ERR_FAILED);
  EXPECT_EQ(delegate_.connection_error->response_code, 500);
}

TEST_F(StreamingWebSocketClientTest, ChannelDropped) {
  base::test::TestFuture<void> future;
  delegate_.on_event_callback = future.GetRepeatingCallback();

  auto conn = EstablishConnection();

  conn.client_remote->OnDropChannel(true, 1000, "Normal closure");

  EXPECT_TRUE(future.Wait());
  ASSERT_TRUE(delegate_.drop_channel.has_value());
  EXPECT_TRUE(delegate_.drop_channel->was_clean);
  EXPECT_EQ(delegate_.drop_channel->code, 1000);
  EXPECT_EQ(delegate_.drop_channel->reason, "Normal closure");
  EXPECT_TRUE(delegate_.drop_channel->elapsed.has_value());

  base::test::TestFuture<void> future_err;
  delegate_.on_event_callback = future_err.GetRepeatingCallback();

  // Sending after disconnection should trigger an error with empty message.
  client_.Send(std::vector<uint8_t>{2});
  EXPECT_TRUE(future_err.Wait());
  ASSERT_TRUE(delegate_.error_message.has_value());
  EXPECT_EQ(*delegate_.error_message, "");
}

TEST_F(StreamingWebSocketClientTest, MessageFragmentation) {
  base::test::TestFuture<void> future;
  delegate_.on_event_callback = future.GetRepeatingCallback();

  auto conn = EstablishConnection();

  std::string full_data = "hello world";
  size_t bytes_written = 0;
  ASSERT_EQ(conn.server_to_client_producer->WriteData(
                base::as_bytes(base::span(full_data)),
                MOJO_WRITE_DATA_FLAG_NONE, bytes_written),
            MOJO_RESULT_OK);
  EXPECT_EQ(bytes_written, full_data.size());

  size_t half_size = full_data.size() / 2;
  conn.client_remote->OnDataFrame(
      false, network::mojom::WebSocketMessageType::BINARY, half_size);

  conn.client_remote->OnDataFrame(
      true, network::mojom::WebSocketMessageType::CONTINUATION,
      full_data.size() - half_size);

  EXPECT_TRUE(future.Wait());
  ASSERT_EQ(delegate_.messages.size(), 1u);
  std::string received(delegate_.messages[0].begin(),
                       delegate_.messages[0].end());
  EXPECT_EQ(received, "hello world");
}

TEST_F(StreamingWebSocketClientTest, NoXClientDataHeader) {
  client_.Send(std::vector<uint8_t>{1});

  EXPECT_TRUE(network_context_.create_called);
  EXPECT_EQ(network_context_.GetHeader("X-WebChannel-Content-Type"),
            "application/x-protobuf");
  EXPECT_FALSE(network_context_.GetHeader("X-Client-Data").has_value());
  EXPECT_FALSE(network_context_.GetHeader("x-client-data").has_value());
}

TEST_F(StreamingWebSocketClientTest, AdditionalHeaders) {
  delegate_.additional_headers.push_back(
      network::mojom::HttpHeader::New("Custom-Header", "custom-value"));
  client_.Send(std::vector<uint8_t>{1});

  EXPECT_TRUE(network_context_.create_called);
  EXPECT_EQ(network_context_.GetHeader("X-WebChannel-Content-Type"),
            "application/x-protobuf");
  EXPECT_EQ(network_context_.GetHeader("Custom-Header"), "custom-value");
}

TEST_F(StreamingWebSocketClientTest, InvalidFrameType) {
  base::test::TestFuture<void> future;
  delegate_.on_event_callback = future.GetRepeatingCallback();

  auto conn = EstablishConnection();

  // Sending a TEXT frame instead of BINARY should trigger an error.
  conn.client_remote->OnDataFrame(
      true, network::mojom::WebSocketMessageType::TEXT, 10);

  EXPECT_TRUE(future.Wait());
  ASSERT_TRUE(delegate_.error_message.has_value());
  EXPECT_NE(delegate_.error_message->find("Invalid WebSocket frame"),
            std::string::npos);
}

TEST_F(StreamingWebSocketClientTest, MessageTooLarge) {
  base::test::TestFuture<void> future;
  delegate_.on_event_callback = future.GetRepeatingCallback();

  auto conn = EstablishConnection();

  // Sending a frame larger than 1 MB should trigger an error.
  conn.client_remote->OnDataFrame(
      true, network::mojom::WebSocketMessageType::BINARY, (1 << 20) + 1);

  EXPECT_TRUE(future.Wait());
  ASSERT_TRUE(delegate_.error_message.has_value());
  EXPECT_NE(delegate_.error_message->find("Invalid WebSocket frame"),
            std::string::npos);
}

TEST_F(StreamingWebSocketClientTest, ExplicitClose) {
  client_.Send(std::vector<uint8_t>{1});
  client_.Close();

  base::test::TestFuture<void> future_err;
  delegate_.on_event_callback = future_err.GetRepeatingCallback();

  client_.Send(std::vector<uint8_t>{2});
  EXPECT_TRUE(future_err.Wait());
  ASSERT_TRUE(delegate_.error_message.has_value());
  EXPECT_EQ(*delegate_.error_message, "");
}

TEST_F(StreamingWebSocketClientTest, OutboundDataVerification) {
  std::vector<uint8_t> initial_payload = {10, 20, 30, 40};
  auto conn = EstablishConnection(initial_payload);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return conn.fake_websocket->is_receiving &&
           conn.fake_websocket->sent_messages.size() == 1u;
  }));

  EXPECT_EQ(conn.fake_websocket->sent_messages[0].type,
            network::mojom::WebSocketMessageType::BINARY);
  EXPECT_EQ(conn.fake_websocket->sent_messages[0].data_length,
            initial_payload.size());

  std::vector<uint8_t> read_buffer(initial_payload.size());
  size_t bytes_read = 0;
  ASSERT_EQ(conn.client_to_server_consumer->ReadData(
                MOJO_READ_DATA_FLAG_NONE, base::span(read_buffer), bytes_read),
            MOJO_RESULT_OK);
  EXPECT_EQ(bytes_read, initial_payload.size());
  EXPECT_EQ(read_buffer, initial_payload);

  // Send additional data while connection is active.
  std::vector<uint8_t> next_payload = {50, 60};
  client_.Send(next_payload);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return conn.fake_websocket->sent_messages.size() == 2u; }));

  EXPECT_EQ(conn.fake_websocket->sent_messages[1].type,
            network::mojom::WebSocketMessageType::BINARY);
  EXPECT_EQ(conn.fake_websocket->sent_messages[1].data_length,
            next_payload.size());

  std::vector<uint8_t> next_read_buffer(next_payload.size());
  bytes_read = 0;
  ASSERT_EQ(
      conn.client_to_server_consumer->ReadData(
          MOJO_READ_DATA_FLAG_NONE, base::span(next_read_buffer), bytes_read),
      MOJO_RESULT_OK);
  EXPECT_EQ(bytes_read, next_payload.size());
  EXPECT_EQ(next_read_buffer, next_payload);
}

TEST_F(StreamingWebSocketClientTest, PreConnectionQueueing) {
  std::vector<uint8_t> msg1 = {1, 2, 3};
  std::vector<uint8_t> msg2 = {4, 5};
  std::vector<uint8_t> msg3 = {6, 7, 8, 9};

  // Buffer multiple messages before connection is established.
  client_.Send(msg1);
  client_.Send(msg2);
  client_.Send(msg3);

  auto conn = CompleteHandshake();

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return conn.fake_websocket->sent_messages.size() == 3u; }));

  EXPECT_EQ(conn.fake_websocket->sent_messages[0].data_length, msg1.size());
  EXPECT_EQ(conn.fake_websocket->sent_messages[1].data_length, msg2.size());
  EXPECT_EQ(conn.fake_websocket->sent_messages[2].data_length, msg3.size());

  std::vector<uint8_t> expected_combined = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  std::vector<uint8_t> read_buffer(expected_combined.size());
  size_t bytes_read = 0;
  ASSERT_EQ(conn.client_to_server_consumer->ReadData(
                MOJO_READ_DATA_FLAG_NONE, base::span(read_buffer), bytes_read),
            MOJO_RESULT_OK);
  EXPECT_EQ(bytes_read, expected_combined.size());
  EXPECT_EQ(read_buffer, expected_combined);
}

TEST_F(StreamingWebSocketClientTest, MojoPipeDisconnect) {
  base::test::TestFuture<void> future;
  delegate_.on_event_callback = future.GetRepeatingCallback();

  auto conn = EstablishConnection();

  // Closing client_remote simulates network service crash or disconnect.
  conn.client_remote.reset();

  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(delegate_.closed);

  // Subsequent Send() should fail immediately.
  base::test::TestFuture<void> future_err;
  delegate_.on_event_callback = future_err.GetRepeatingCallback();
  client_.Send(std::vector<uint8_t>{1});
  EXPECT_TRUE(future_err.Wait());
  ASSERT_TRUE(delegate_.error_message.has_value());
  EXPECT_EQ(*delegate_.error_message, "");
}

TEST_F(StreamingWebSocketClientTest, DataPipeWriteFailure) {
  base::test::TestFuture<void> future;
  delegate_.on_event_callback = future.GetRepeatingCallback();

  auto conn = EstablishConnection();

  // Reset the consumer handle so writing into the producer data pipe fails.
  conn.client_to_server_consumer.reset();

  client_.Send(std::vector<uint8_t>{1, 2, 3});

  EXPECT_TRUE(future.Wait());
  ASSERT_TRUE(delegate_.error_message.has_value());
  EXPECT_EQ(*delegate_.error_message, "Failed to write to WebSocket.");
}

TEST_F(StreamingWebSocketClientTest, ServiceUrl) {
  EXPECT_EQ(client_.service_url(), GURL("wss://example.com/websocket"));
}

}  // namespace

}  // namespace streaming_client

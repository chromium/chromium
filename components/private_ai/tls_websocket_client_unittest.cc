// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/tls_websocket_client.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/proto/google_rpc_code.pb.h"
#include "components/private_ai/proto/private_tls_service.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
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
      network::mojom::IPAddressSpace target_address_space,
      std::optional<int64_t> target_network) override {
    create_called_ = true;
    pending_handshake_client_ = std::move(handshake_client);
  }

  bool create_called_ = false;
  mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
      pending_handshake_client_;
};

class TlsWebSocketClientTest : public ::testing::Test {
 protected:
  TlsWebSocketClientTest()
      : client_(GURL("wss://example.com/tls"),
                GetNetworkContextGetter(),
                &logger_,
                /*has_chrome_proxy=*/false) {
    client_.SetFrameCallback(future_.GetRepeatingCallback());
  }

  network::NetworkContextGetter GetNetworkContextGetter() {
    return base::BindRepeating(
        [](network::mojom::NetworkContext* context) { return context; },
        base::Unretained(&network_context_));
  }

  base::test::TaskEnvironment task_environment_;
  PrivateAiLogger logger_;
  MockNetworkContext network_context_;
  base::test::TestFuture<
      base::expected<std::vector<uint8_t>, TlsWebSocketClient::Error>>
      future_;
  TlsWebSocketClient client_;
};

TEST_F(TlsWebSocketClientTest, SendFrameSerializesTlsSessionRequest) {
  base::HistogramTester histogram_tester;
  const std::vector<uint8_t> outgoing_frame = {0x16, 0x03, 0x01, 0xAA};
  client_.SendFrame(outgoing_frame);

  EXPECT_TRUE(network_context_.create_called_);
  ASSERT_TRUE(network_context_.pending_handshake_client_.is_valid());

  mojo::Remote<network::mojom::WebSocketHandshakeClient> handshake_client(
      std::move(network_context_.pending_handshake_client_));

  mojo::Remote<network::mojom::WebSocketClient> client_remote;
  auto client_receiver = client_remote.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<network::mojom::WebSocket> websocket_remote;
  auto websocket_receiver = websocket_remote.InitWithNewPipeAndPassReceiver();

  mojo::ScopedDataPipeProducerHandle server_to_client_producer;
  mojo::ScopedDataPipeConsumerHandle readable_consumer;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, server_to_client_producer,
                                 readable_consumer),
            MOJO_RESULT_OK);

  mojo::ScopedDataPipeProducerHandle writable_producer;
  mojo::ScopedDataPipeConsumerHandle client_to_server_consumer;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, writable_producer,
                                 client_to_server_consumer),
            MOJO_RESULT_OK);

  handshake_client->OnConnectionEstablished(
      std::move(websocket_remote), std::move(client_receiver),
      network::mojom::WebSocketHandshakeResponse::New(),
      std::move(readable_consumer), std::move(writable_producer));

  proto::TlsSessionRequest expected_req;
  expected_req.set_frame(outgoing_frame.data(), outgoing_frame.size());
  std::string expected_bytes;
  ASSERT_TRUE(expected_req.SerializeToString(&expected_bytes));

  std::vector<uint8_t> received(expected_bytes.size());
  size_t bytes_read = 0;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return client_to_server_consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                                               received,
                                               bytes_read) == MOJO_RESULT_OK;
  }));
  EXPECT_EQ(bytes_read, expected_bytes.size());
  histogram_tester.ExpectUniqueSample(
      "PrivateAi.Client.ConnectionResult.WithoutProxy", net::OK, 1);

  proto::TlsSessionRequest parsed_req;
  ASSERT_TRUE(
      parsed_req.ParseFromArray(received.data(), static_cast<int>(bytes_read)));
  EXPECT_EQ(parsed_req.frame(),
            std::string(outgoing_frame.begin(), outgoing_frame.end()));
}

TEST_F(TlsWebSocketClientTest, SuccessfulResponse) {
  proto::TlsSessionResponse response;
  response.set_frame("test_tls_frame_data");
  std::string serialized;
  ASSERT_TRUE(response.SerializeToString(&serialized));
  std::vector<uint8_t> message(serialized.begin(), serialized.end());

  client_.OnMessage(std::move(message));

  auto result = future_.Take();
  ASSERT_TRUE(result.has_value());
  std::string received_frame(result->begin(), result->end());
  EXPECT_EQ(received_frame, "test_tls_frame_data");
}

TEST_F(TlsWebSocketClientTest, DeserializationError) {
  client_.OnMessage({0xFF, 0xFF, 0xFF, 0xFF});

  auto result = future_.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), TlsWebSocketClient::Error::kDeserializationError);
}

TEST_F(TlsWebSocketClientTest, ChannelDropped) {
  base::HistogramTester histogram_tester;

  client_.OnDropChannel(/*was_clean=*/true, /*code=*/1000,
                        "generic::unavailable: test",
                        /*elapsed=*/base::Seconds(5));

  auto result = future_.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), TlsWebSocketClient::Error::kSocketClosed);
  histogram_tester.ExpectUniqueSample("PrivateAi.Client.WebSocketCloseCode",
                                      1000, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "PrivateAi.Client.WebSocketSessionDuration.ClosedByServer",
      base::Seconds(5), 1);
  histogram_tester.ExpectUniqueSample("PrivateAi.Client.ServerStatusCode",
                                      rpc::GoogleRpcCode::UNAVAILABLE, 1);
}

TEST_F(TlsWebSocketClientTest, ChannelDroppedWithoutReasonOrDuration) {
  base::HistogramTester histogram_tester;

  client_.OnDropChannel(/*was_clean=*/false, /*code=*/1006, /*reason=*/"",
                        /*elapsed=*/std::nullopt);

  auto result = future_.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), TlsWebSocketClient::Error::kSocketClosed);
  histogram_tester.ExpectUniqueSample("PrivateAi.Client.WebSocketCloseCode",
                                      1006, 1);
  histogram_tester.ExpectTotalCount(
      "PrivateAi.Client.WebSocketSessionDuration.ClosedByServer", 0);
  histogram_tester.ExpectTotalCount("PrivateAi.Client.ServerStatusCode", 0);
}

TEST_F(TlsWebSocketClientTest, ConnectionErrorReportsConnectionFailed) {
  base::HistogramTester histogram_tester;
  client_.OnConnectionError("Net error", net::ERR_NAME_NOT_RESOLVED, 500);

  auto result = future_.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), TlsWebSocketClient::Error::kConnectionFailed);
  histogram_tester.ExpectUniqueSample(
      "PrivateAi.Client.WebSocketConnectionError.NetError",
      -net::ERR_NAME_NOT_RESOLVED, 1);
  histogram_tester.ExpectUniqueSample(
      "PrivateAi.Client.WebSocketConnectionError.ResponseCode", 500, 1);
  histogram_tester.ExpectUniqueSample(
      "PrivateAi.Client.ConnectionResult.WithoutProxy",
      -net::ERR_NAME_NOT_RESOLVED, 1);
}

TEST_F(TlsWebSocketClientTest, ConnectionErrorWithoutResponseCode) {
  base::HistogramTester histogram_tester;
  client_.OnConnectionError("Net error", net::ERR_CONNECTION_TIMED_OUT, -1);

  auto result = future_.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), TlsWebSocketClient::Error::kConnectionFailed);
  histogram_tester.ExpectUniqueSample(
      "PrivateAi.Client.WebSocketConnectionError.NetError",
      -net::ERR_CONNECTION_TIMED_OUT, 1);
  histogram_tester.ExpectTotalCount(
      "PrivateAi.Client.WebSocketConnectionError.ResponseCode", 0);
}

TEST_F(TlsWebSocketClientTest, ConnectionResultWithProxyAndIdempotency) {
  base::HistogramTester histogram_tester;
  TlsWebSocketClient proxy_client(GURL("wss://example.com/tls"),
                                  GetNetworkContextGetter(), &logger_,
                                  /*has_chrome_proxy=*/true);
  proxy_client.SetFrameCallback(base::DoNothing());

  proxy_client.OnConnected();
  // Subsequent connection events should not record a second sample.
  proxy_client.OnConnectionError("Late error", net::ERR_FAILED, -1);

  histogram_tester.ExpectUniqueSample(
      "PrivateAi.Client.ConnectionResult.WithProxy", net::OK, 1);
  histogram_tester.ExpectTotalCount(
      "PrivateAi.Client.ConnectionResult.WithoutProxy", 0);
}

TEST_F(TlsWebSocketClientTest, ConnectionResultNotRecordedWhenProxyUnset) {
  base::HistogramTester histogram_tester;
  TlsWebSocketClient default_client(GURL("wss://example.com/tls"),
                                    GetNetworkContextGetter(), &logger_);
  default_client.SetFrameCallback(base::DoNothing());

  default_client.OnConnected();

  histogram_tester.ExpectTotalCount(
      "PrivateAi.Client.ConnectionResult.WithProxy", 0);
  histogram_tester.ExpectTotalCount(
      "PrivateAi.Client.ConnectionResult.WithoutProxy", 0);
}

TEST_F(TlsWebSocketClientTest, OnErrorReportsError) {
  client_.OnError("Fatal error");

  auto result = future_.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), TlsWebSocketClient::Error::kError);
}

TEST_F(TlsWebSocketClientTest, SubsequentEventsAfterDisconnectDoNotFireAgain) {
  client_.OnClose();
  auto result = future_.Take();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), TlsWebSocketClient::Error::kSocketClosed);

  client_.OnError("Ignored after disconnect");
  client_.SendFrame({1, 2, 3});
  base::test::TestFuture<void> flush;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           flush.GetCallback());
  EXPECT_TRUE(flush.Wait());
  EXPECT_FALSE(future_.IsReady());
}

}  // namespace
}  // namespace private_ai

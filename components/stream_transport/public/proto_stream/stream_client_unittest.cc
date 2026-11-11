// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/public/proto_stream/stream_client.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/stream_transport/public/proto_stream/message_stream_client.h"
#include "components/stream_transport/public/proto_stream/oauth_stream_connection_delegate.h"
#include "components/stream_transport/public/proto_stream/stream_connection_delegate.h"
#include "components/stream_transport/public/proto_stream/stream_framer.h"
#include "components/stream_transport/test_support/proto_stream/mock_message_stream_client.h"
#include "components/stream_transport/test_support/proto_stream/mock_stream_connection_delegate.h"
#include "components/stream_transport/test_support/proto_stream/mock_stream_framer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace stream_transport {
namespace {

using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

constexpr char kEndpoint[] = "https://example.com/v1/stream?alt=proto";
constexpr signin::OAuthConsumerId kTestConsumerId =
    signin::OAuthConsumerId::kFeedNetwork;

// Encodes `payload` as a length-delimited `google.rpc.StreamBody.message`
// field (field number 1, wire type 2 -> tag byte 0x0a) for a payload < 128 B.
std::string EncodeStreamBodyMessageFrame(std::string_view payload) {
  CHECK_LT(payload.size(), 128u);
  std::string frame;
  frame.push_back(0x0a);
  frame.push_back(static_cast<char>(payload.size()));
  frame.append(payload);
  return frame;
}

network::mojom::URLResponseHeadPtr MakeProtobufResponseHead() {
  auto head = network::CreateURLResponseHead(net::HTTP_OK);
  head->mime_type = "application/x-protobuf";
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 200 OK\nContent-Type: application/x-protobuf\n\n"));
  return head;
}

TEST(StreamClientPublicApiTest, CreateRustStreamFramerParsesStreamBodyFrame) {
  std::unique_ptr<StreamFramer> framer = CreateRustStreamFramer();
  ASSERT_TRUE(framer);

  StreamFramer::FeedResult result =
      framer->Feed(EncodeStreamBodyMessageFrame("hello-world"));
  EXPECT_FALSE(result.failed);
  EXPECT_THAT(result.messages, ElementsAre("hello-world"));
  EXPECT_FALSE(result.status.has_value());
}

TEST(StreamClientPublicApiTest,
     CreateResumeBodyConnectionDelegateDefaultsInnerToPassThrough) {
  std::unique_ptr<StreamConnectionDelegate> delegate =
      CreateResumeBodyConnectionDelegate(
          base::BindRepeating([]() { return std::string("resume-bytes"); }));
  ASSERT_TRUE(delegate);

  std::optional<StreamUploadBody> body = delegate->GetConnectionRequestBody();
  ASSERT_TRUE(body.has_value());
  EXPECT_EQ(body->content, "resume-bytes");
  EXPECT_EQ(body->content_type, "application/x-protobuf");

  base::test::TestFuture<std::unique_ptr<network::ResourceRequest>> future;
  delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                           future.GetCallback());
  EXPECT_TRUE(future.Take());
}

TEST(StreamClientPublicApiTest,
     CreateOAuthStreamConnectionDelegateSupportsOptionalHistogramAndNullInner) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakePrimaryAccountAvailable("user@example.com",
                                                signin::ConsentLevel::kSignin);

  // Default empty histogram name records no metric and accepts a null inner
  // delegate.
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<StreamConnectionDelegate> delegate =
        CreateOAuthStreamConnectionDelegate(
            /*inner=*/nullptr, identity_test_env.identity_manager(),
            kTestConsumerId);

    base::test::TestFuture<std::unique_ptr<network::ResourceRequest>> future;
    delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                             future.GetCallback());
    identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        "token-1", base::Time::Now() + base::Hours(1));
    std::unique_ptr<network::ResourceRequest> request = future.Take();
    ASSERT_TRUE(request);
    EXPECT_EQ(
        request->headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
        "Bearer token-1");
    histogram_tester.ExpectTotalCount("StreamTransport.OAuth.TokenFetchLatency",
                                      0);
  }

  // Explicit histogram name records fetch latency under the caller's metric.
  {
    base::HistogramTester histogram_tester;
    std::unique_ptr<StreamConnectionDelegate> delegate =
        CreateOAuthStreamConnectionDelegate(
            /*inner=*/nullptr, identity_test_env.identity_manager(),
            kTestConsumerId, "CustomConsumer.OAuth.TokenFetchLatency");

    base::test::TestFuture<std::unique_ptr<network::ResourceRequest>> future;
    delegate->PrepareRequest(std::make_unique<network::ResourceRequest>(),
                             future.GetCallback());
    task_environment.FastForwardBy(base::Milliseconds(120));
    identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        "token-2", base::Time::Now() + base::Hours(1));
    ASSERT_TRUE(future.Take());
    histogram_tester.ExpectUniqueTimeSample(
        "CustomConsumer.OAuth.TokenFetchLatency", base::Milliseconds(120), 1);
  }
}

TEST(StreamClientPublicApiTest,
     CreateProtoStreamClientDeliversFramedMessagesAndHonorsConfig) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO};
  network::TestURLLoaderFactory test_url_loader_factory;
  std::vector<network::ResourceRequest> observed_requests;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        observed_requests.push_back(request);
      }));

  test_url_loader_factory.AddResponse(
      GURL(kEndpoint), MakeProtobufResponseHead(),
      EncodeStreamBodyMessageFrame("step-payload"),
      network::URLLoaderCompletionStatus(net::OK));

  base::test::TestFuture<std::string> message_future;
  NiceMock<MockMessageStreamObserver> observer;
  ON_CALL(observer, OnStreamMessage)
      .WillByDefault([&](const std::string& message) {
        if (!message_future.IsReady()) {
          message_future.SetValue(message);
        }
      });

  ProtoStreamClientConfig config;
  config.base_reconnection_time = base::Seconds(11);
  config.max_reconnection_time = base::Seconds(22);
  config.stall_timeout = base::Minutes(1);

  std::unique_ptr<MessageStreamClient> client = CreateProtoStreamClient(
      test_url_loader_factory.GetSafeWeakWrapper(), GURL(kEndpoint),
      CreateResumeBodyConnectionDelegate(
          base::BindRepeating([]() { return std::string("req-body"); })),
      TRAFFIC_ANNOTATION_FOR_TESTS, config);
  client->AddObserver(&observer);
  client->Connect();

  EXPECT_EQ(message_future.Get(), "step-payload");
  ASSERT_EQ(observed_requests.size(), 1u);
  EXPECT_EQ(observed_requests[0].method, "POST");
  EXPECT_EQ(network::GetUploadData(observed_requests[0]), "req-body");

  // After the non-terminal HTTP 200 stream completes, the client schedules a
  // reconnect using `config.base_reconnection_time` (11s), not the default 3s.
  task_environment.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(observed_requests.size(), 1u);
  task_environment.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(observed_requests.size(), 2u);

  client->Disconnect();
  client->RemoveObserver(&observer);
}

TEST(StreamClientPublicApiTest,
     CreateProtoStreamClientAcceptsCustomStreamFramerFactory) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO};
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.AddResponse(
      GURL(kEndpoint), MakeProtobufResponseHead(), "raw-wire-chunk",
      network::URLLoaderCompletionStatus(net::OK));

  base::test::TestFuture<std::string> message_future;
  NiceMock<MockMessageStreamObserver> observer;
  ON_CALL(observer, OnStreamMessage)
      .WillByDefault([&](const std::string& message) {
        message_future.SetValue(message);
      });

  StreamFramerFactory custom_framer_factory =
      base::BindRepeating([]() -> std::unique_ptr<StreamFramer> {
        auto framer = std::make_unique<StrictMock<MockStreamFramer>>();
        EXPECT_CALL(*framer, Feed(std::string_view("raw-wire-chunk")))
            .WillOnce([]() {
              StreamFramer::FeedResult result;
              result.messages.push_back("custom-framed-message");
              return result;
            });
        return framer;
      });

  std::unique_ptr<MessageStreamClient> client = CreateProtoStreamClient(
      test_url_loader_factory.GetSafeWeakWrapper(), GURL(kEndpoint),
      std::make_unique<DefaultStreamConnectionDelegate>(),
      std::move(custom_framer_factory), TRAFFIC_ANNOTATION_FOR_TESTS);
  client->AddObserver(&observer);
  client->Connect();

  EXPECT_EQ(message_future.Get(), "custom-framed-message");
  client->Disconnect();
  client->RemoveObserver(&observer);
}

}  // namespace
}  // namespace stream_transport

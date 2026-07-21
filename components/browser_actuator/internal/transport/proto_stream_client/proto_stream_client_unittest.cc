// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/proto_stream_client/proto_stream_client.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport/stream_framer.h"
#include "components/browser_actuator/internal/transport/test_support/mock_stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport/test_support/mock_stream_framer.h"
#include "components/browser_actuator/internal/transport/test_support/wait_for.h"
#include "mojo/public/cpp/system/data_pipe.h"
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

namespace browser_actuator {
namespace {

using testing::ElementsAre;
using testing::NiceMock;

constexpr char kEndpoint[] = "https://example.com/v1/stream?alt=proto";

network::mojom::URLResponseHeadPtr MakeResponseHead(
    const std::string& mime_type,
    net::HttpStatusCode code = net::HTTP_OK) {
  auto head = network::CreateURLResponseHead(code);
  head->mime_type = mime_type;
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(
          "HTTP/1.1 " + base::NumberToString(code) +
          " Status\nContent-Type: " + mime_type + "\n\n"));
  return head;
}

class RecordingObserver : public MessageStreamClient::Observer {
 public:
  void OnStreamMessage(const std::string& message) override {
    messages.push_back(message);
    events.push_back("message:" + message);
    if (on_message) {
      on_message.Run(message);
    }
  }
  void OnStreamStatus(const std::string& status) override {
    statuses.push_back(status);
    events.push_back("status:" + status);
  }
  void OnStreamConnectionStateChange(bool connected) override {
    state_changes.push_back(connected);
    events.push_back(connected ? "connected" : "disconnected");
    if (on_state_change) {
      on_state_change.Run(connected);
    }
  }

  std::vector<std::string> messages;
  std::vector<std::string> statuses;
  std::vector<bool> state_changes;
  // Every notification in arrival order; lets tests assert cross-type
  // ordering, e.g. that a message precedes the disconnect state change.
  std::vector<std::string> events;
  base::RepeatingCallback<void(const std::string&)> on_message;
  base::RepeatingCallback<void(bool)> on_state_change;
};

class ProtoStreamClientTest : public testing::Test {
 protected:
  ProtoStreamClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          base::test::TaskEnvironment::MainThreadType::IO) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [this](const network::ResourceRequest& request) {
          requests_.push_back(request);
        }));
    // Unless a test overrides it, a minted framer frames every received
    // chunk as one complete message.
    feed_handler_ = base::BindRepeating([](const std::string& chunk) {
      StreamFramer::FeedResult result;
      result.messages.push_back(chunk);
      return result;
    });
  }

  std::unique_ptr<ProtoStreamClient> MakeClient(
      std::unique_ptr<StreamConnectionDelegate> delegate =
          std::make_unique<DefaultStreamConnectionDelegate>()) {
    return std::make_unique<ProtoStreamClient>(
        test_url_loader_factory_.GetSafeWeakWrapper(), GURL(kEndpoint),
        std::move(delegate),
        base::BindRepeating(&ProtoStreamClientTest::MakeFramer,
                            base::Unretained(this)),
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // Mints the mock framer handed to the client; every framer feeds
  // received bytes into `fed_bytes_` and answers via `feed_handler_`.
  std::unique_ptr<StreamFramer> MakeFramer() {
    auto framer = std::make_unique<NiceMock<MockStreamFramer>>();
    ON_CALL(*framer, Feed).WillByDefault([this](std::string_view chunk) {
      fed_bytes_ += chunk;
      return feed_handler_.Run(std::string(chunk));
    });
    ++framers_minted_;
    return framer;
  }

  // Serves `body` as a valid proto stream that ends (cleanly) after the
  // body is delivered.
  void ServeStream(const std::string& body) {
    test_url_loader_factory_.AddResponse(
        GURL(kEndpoint), MakeResponseHead("application/x-protobuf"), body,
        network::URLLoaderCompletionStatus(net::OK));
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::vector<network::ResourceRequest> requests_;
  base::RepeatingCallback<StreamFramer::FeedResult(const std::string&)>
      feed_handler_;
  std::string fed_bytes_;
  int framers_minted_ = 0;
};

TEST_F(ProtoStreamClientTest, ConnectReceivesMessages) {
  ServeStream("stream body bytes");
  feed_handler_ = base::BindRepeating([](const std::string& chunk) {
    StreamFramer::FeedResult result;
    result.messages = {"first proto", "second proto"};
    return result;
  });

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  client->Connect();
  // Both state changes (connected, then stream over) imply both messages
  // were delivered in between.
  ASSERT_TRUE(WaitFor([&] { return observer.state_changes.size() >= 2u; }));

  EXPECT_THAT(observer.messages, ElementsAre("first proto", "second proto"));
  EXPECT_TRUE(observer.statuses.empty());
  EXPECT_THAT(observer.state_changes, ElementsAre(true, false));
  EXPECT_FALSE(client->IsConnected());

  ASSERT_EQ(requests_.size(), 1u);
  EXPECT_EQ(requests_[0].method, "GET");
  EXPECT_EQ(requests_[0].headers.GetHeader("Accept"), "application/x-protobuf");
  EXPECT_EQ(requests_[0].credentials_mode,
            network::mojom::CredentialsMode::kOmit);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, FramerReceivesTheRawBody) {
  const std::string body = "opaque stream body bytes";
  ServeStream(body);

  auto client = MakeClient();
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return fed_bytes_ == body; }));
  EXPECT_EQ(framers_minted_, 1);
}

TEST_F(ProtoStreamClientTest, StatusIsDelivered) {
  ServeStream("body");
  feed_handler_ = base::BindRepeating([](const std::string& chunk) {
    StreamFramer::FeedResult result;
    result.messages = {"last message"};
    result.status = "serialized rpc status";
    return result;
  });

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return observer.state_changes.size() >= 2u; }));

  EXPECT_THAT(observer.messages, ElementsAre("last message"));
  EXPECT_THAT(observer.statuses, ElementsAre("serialized rpc status"));

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, DelegateSeesMessagesBeforeObservers) {
  ServeStream("one chunk");

  std::vector<std::string> log;
  auto delegate = std::make_unique<NiceMock<MockStreamConnectionDelegate>>();
  ON_CALL(*delegate, PrepareRequest)
      .WillByDefault(
          [](std::unique_ptr<network::ResourceRequest> request,
             StreamConnectionDelegate::PrepareRequestCallback callback) {
            std::move(callback).Run(std::move(request));
          });
  ON_CALL(*delegate, OnMessageDispatched)
      .WillByDefault([&](const std::string& message) {
        log.push_back("delegate:" + message);
      });

  RecordingObserver observer;
  observer.on_message =
      base::BindLambdaForTesting([&](const std::string& message) {
        log.push_back("observer:" + message);
      });

  auto client = MakeClient(std::move(delegate));
  client->AddObserver(&observer);
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return !observer.messages.empty(); }));

  EXPECT_THAT(log, ElementsAre("delegate:one chunk", "observer:one chunk"));

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, WrongContentTypeFailsWithoutFraming) {
  test_url_loader_factory_.AddResponse(
      GURL(kEndpoint), MakeResponseHead("text/html"), "<html>nope</html>",
      network::URLLoaderCompletionStatus(net::OK));

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  client->Connect();
  // Nothing observable ever turns true on a rejection, so there is no
  // condition to wait for (and RunUntilIdle is banned in tests): drain
  // the attempt by fast-forwarding far past any plausible internal
  // delay, which doubles as proof that nothing fires later either.
  task_environment_.FastForwardBy(base::Minutes(30));

  EXPECT_TRUE(observer.messages.empty());
  EXPECT_TRUE(observer.state_changes.empty());
  EXPECT_FALSE(client->IsConnected());
  // No framer was ever minted: untrusted bytes of a rejected response
  // must not reach the framing parser.
  EXPECT_EQ(framers_minted_, 0);
  EXPECT_EQ(requests_.size(), 1u);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, HttpErrorFailsWithoutFraming) {
  test_url_loader_factory_.AddResponse(
      GURL(kEndpoint),
      MakeResponseHead("application/x-protobuf", net::HTTP_NOT_FOUND), "",
      network::URLLoaderCompletionStatus(net::OK));

  auto client = MakeClient();
  client->Connect();
  task_environment_.FastForwardBy(base::Minutes(30));

  EXPECT_FALSE(client->IsConnected());
  EXPECT_EQ(framers_minted_, 0);
  EXPECT_EQ(requests_.size(), 1u);
}

TEST_F(ProtoStreamClientTest, MalformedFramingFailsPermanently) {
  ServeStream("not valid framing");
  feed_handler_ = base::BindRepeating([](const std::string& chunk) {
    StreamFramer::FeedResult result;
    result.failed = true;
    return result;
  });

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return observer.state_changes.size() >= 2u; }));

  EXPECT_TRUE(observer.messages.empty());
  EXPECT_THAT(observer.state_changes, ElementsAre(true, false));
  EXPECT_FALSE(client->IsConnected());

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, ObserverDisconnectStopsBatchDelivery) {
  ServeStream("body");
  feed_handler_ = base::BindRepeating([](const std::string& chunk) {
    StreamFramer::FeedResult result;
    result.messages = {"one", "two"};
    result.status = "late status";
    return result;
  });

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  observer.on_message = base::BindLambdaForTesting(
      [&](const std::string& message) { client->Disconnect(); });
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return !observer.messages.empty(); }));

  // Disconnect() draws the line at the current notification's boundary:
  // the rest of the batch — the second message and even the terminal
  // status framed in the same chunk — belongs to a stream the client no
  // longer owns. (An observer that wants the status should simply not
  // disconnect; a delivered terminal status already stops the client.)
  EXPECT_THAT(observer.events,
              ElementsAre("connected", "message:one", "disconnected"));
  EXPECT_TRUE(observer.statuses.empty());
  EXPECT_FALSE(client->IsConnected());

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, ObserverDisconnectDeliversInFlightMessageToAll) {
  ServeStream("body");
  feed_handler_ = base::BindRepeating([](const std::string& chunk) {
    StreamFramer::FeedResult result;
    result.messages = {"one", "two"};
    return result;
  });

  RecordingObserver first_observer;
  RecordingObserver second_observer;
  auto client = MakeClient();
  client->AddObserver(&first_observer);
  client->AddObserver(&second_observer);
  first_observer.on_message = base::BindLambdaForTesting(
      [&](const std::string& message) { client->Disconnect(); });
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return second_observer.events.size() >= 3u; }));

  // The teardown requested by the first observer is deferred to the
  // notification boundary: the in-flight message still reaches every
  // observer, each seeing it before the disconnect state change, and the
  // rest of the batch is dropped.
  EXPECT_THAT(first_observer.events,
              ElementsAre("connected", "message:one", "disconnected"));
  EXPECT_THAT(second_observer.events,
              ElementsAre("connected", "message:one", "disconnected"));
  EXPECT_FALSE(client->IsConnected());

  client->RemoveObserver(&first_observer);
  client->RemoveObserver(&second_observer);
}

TEST_F(ProtoStreamClientTest, ObserverDisconnectFromConnectNotification) {
  ServeStream("body");

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  observer.on_state_change = base::BindLambdaForTesting([&](bool connected) {
    if (connected) {
      client->Disconnect();
    }
  });
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return observer.state_changes.size() >= 2u; }));

  // Disconnecting from the "connected" notification tears down at the
  // notification boundary: no message from the doomed stream is ever
  // delivered.
  EXPECT_THAT(observer.events, ElementsAre("connected", "disconnected"));
  EXPECT_TRUE(observer.messages.empty());
  EXPECT_FALSE(client->IsConnected());

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, ObserverDisconnectThenConnectRestartsStream) {
  ServeStream("body");

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  bool restarted = false;
  observer.on_message =
      base::BindLambdaForTesting([&](const std::string& message) {
        if (!restarted) {
          restarted = true;
          client->Disconnect();
          client->Connect();
        }
      });
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return observer.events.size() >= 6u; }));

  // Disconnect() + Connect() from one notification is a restart: the old
  // stream still tears down at the notification boundary, and a fresh
  // connection with a fresh framer starts right after. The second pass
  // runs to its natural end.
  EXPECT_THAT(observer.events,
              ElementsAre("connected", "message:body", "disconnected",
                          "connected", "message:body", "disconnected"));
  EXPECT_EQ(requests_.size(), 2u);
  EXPECT_EQ(framers_minted_, 2);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, ConnectWhileActiveIsNoOp) {
  ServeStream("body");

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  client->Connect();
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return observer.state_changes.size() >= 2u; }));

  EXPECT_EQ(requests_.size(), 1u);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, DelegateAbortIsFailedAttempt) {
  ServeStream("body");
  int prepare_calls = 0;
  auto delegate = std::make_unique<NiceMock<MockStreamConnectionDelegate>>();
  ON_CALL(*delegate, PrepareRequest)
      .WillByDefault(
          [&](std::unique_ptr<network::ResourceRequest> request,
              StreamConnectionDelegate::PrepareRequestCallback callback) {
            // The first attempt has no token to offer; the second does.
            if (++prepare_calls == 1) {
              std::move(callback).Run(nullptr);
            } else {
              std::move(callback).Run(std::move(request));
            }
          });

  RecordingObserver observer;
  auto client = MakeClient(std::move(delegate));
  client->AddObserver(&observer);
  client->Connect();
  EXPECT_TRUE(requests_.empty());

  // The aborted attempt counts as a failure: the retry comes after the
  // base reconnection time and succeeds.
  task_environment_.FastForwardBy(base::Seconds(3));
  ASSERT_TRUE(WaitFor([&] { return !observer.messages.empty(); }));
  EXPECT_EQ(prepare_calls, 2);
  EXPECT_EQ(requests_.size(), 1u);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, StreamEndWithoutStatusReconnects) {
  ServeStream("body");

  auto client = MakeClient();
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return !requests_.empty(); }));

  // An unexpectedly dropped stream (no terminal status) reconnects after
  // the base reconnection time.
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_GE(requests_.size(), 2u);
}

TEST_F(ProtoStreamClientTest, ObserverDisconnectFromStreamEndStopsReconnect) {
  ServeStream("body");

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  observer.on_state_change = base::BindLambdaForTesting([&](bool connected) {
    if (!connected) {
      client->Disconnect();
    }
  });
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return observer.state_changes.size() >= 2u; }));

  // The ended stream would normally schedule a reconnect, but the
  // observer answered the stream-end notification with Disconnect():
  // reconnecting anyway would override that explicit request.
  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_EQ(requests_.size(), 1u);
  EXPECT_FALSE(client->IsConnected());

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, NetworkErrorReconnectsWithBackoff) {
  test_url_loader_factory_.AddResponse(
      GURL(kEndpoint), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));

  auto client = MakeClient();
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return !requests_.empty(); }));

  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(requests_.size(), 2u);

  // Second retry backs off to 6s.
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(requests_.size(), 2u);
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(requests_.size(), 3u);
}

TEST_F(ProtoStreamClientTest, ReconnectDelayHonorsFeatureParams) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kBrowserActuatorProtoStreamTransport,
      {{"ProtoStreamBaseReconnectionTime", "10s"}});

  test_url_loader_factory_.AddResponse(
      GURL(kEndpoint), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));

  auto client = MakeClient();
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return !requests_.empty(); }));

  // At the configured base delay of 10s, the default 3s must pass without
  // a retry; the retry fires at 10s.
  task_environment_.FastForwardBy(base::Seconds(9));
  EXPECT_EQ(requests_.size(), 1u);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(requests_.size(), 2u);
}

TEST_F(ProtoStreamClientTest, BackoffIsCapped) {
  test_url_loader_factory_.AddResponse(
      GURL(kEndpoint), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));

  auto client = MakeClient();
  client->Connect();
  // Drain well past the point where the backoff reaches its ceiling.
  task_environment_.FastForwardBy(base::Minutes(30));
  const size_t steady_state_requests = requests_.size();

  // In steady state the retry cadence is exactly the 5-minute cap: any
  // 10-minute window contains exactly two attempts.
  task_environment_.FastForwardBy(base::Minutes(10));
  EXPECT_EQ(requests_.size(), steady_state_requests + 2);
}

TEST_F(ProtoStreamClientTest, DelegateRetriesOnHttpFailure) {
  test_url_loader_factory_.AddResponse(
      GURL(kEndpoint),
      MakeResponseHead("application/x-protobuf", net::HTTP_UNAUTHORIZED), "",
      network::URLLoaderCompletionStatus(net::OK));

  std::vector<int> rejected_codes;
  auto delegate = std::make_unique<NiceMock<MockStreamConnectionDelegate>>();
  ON_CALL(*delegate, PrepareRequest)
      .WillByDefault(
          [](std::unique_ptr<network::ResourceRequest> request,
             StreamConnectionDelegate::PrepareRequestCallback callback) {
            std::move(callback).Run(std::move(request));
          });
  ON_CALL(*delegate, ShouldRetryOnHttpFailure)
      .WillByDefault([&](int response_code) {
        rejected_codes.push_back(response_code);
        // Retry once, e.g. after invalidating a stale OAuth token.
        return rejected_codes.size() == 1u;
      });

  RecordingObserver observer;
  auto client = MakeClient(std::move(delegate));
  client->AddObserver(&observer);
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return !rejected_codes.empty(); }));

  // The endpoint recovers before the retry fires.
  ServeStream("recovered");
  task_environment_.FastForwardBy(base::Seconds(3));
  ASSERT_TRUE(WaitFor([&] { return !observer.messages.empty(); }));

  EXPECT_THAT(rejected_codes, ElementsAre(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(requests_.size(), 2u);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, ConnectAfterPermanentFailureStartsOver) {
  test_url_loader_factory_.AddResponse(
      GURL(kEndpoint), MakeResponseHead("text/html"), "nope",
      network::URLLoaderCompletionStatus(net::OK));

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  client->Connect();
  task_environment_.FastForwardBy(base::Minutes(30));
  ASSERT_EQ(requests_.size(), 1u);

  // An explicit Connect() after a permanent failure starts over.
  ServeStream("recovered");
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return !observer.messages.empty(); }));
  EXPECT_EQ(requests_.size(), 2u);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, TerminalStatusStopsReconnecting) {
  ServeStream("body");
  feed_handler_ = base::BindRepeating([](const std::string& chunk) {
    StreamFramer::FeedResult result;
    result.messages = {"last message"};
    result.status = "serialized rpc status";
    return result;
  });

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  client->Connect();
  // The status is delivered before the server closes the stream; wait for
  // the disconnect so the whole shutdown sequence has run.
  ASSERT_TRUE(WaitFor([&] { return observer.state_changes.size() >= 2u; }));

  EXPECT_THAT(observer.statuses, ElementsAre("serialized rpc status"));
  EXPECT_FALSE(client->IsConnected());

  // A completed RPC is not retried automatically.
  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_EQ(requests_.size(), 1u);

  // But an explicit Connect() starts over.
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return requests_.size() >= 2u; }));

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, StallWatchdogReconnectsSilentStream) {
  const base::TimeDelta stall_timeout = kProtoStreamStallTimeout.Get();
  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  client->Connect();
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

  // Open a valid proto stream by hand and keep it open without
  // completing: AddResponse() would deliver and finish the response, but
  // the watchdog needs a live-but-silent stream.
  network::TestURLLoaderFactory::PendingRequest* pending =
      test_url_loader_factory_.GetPendingRequest(0);
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer, consumer), MOJO_RESULT_OK);
  pending->client->OnReceiveResponse(MakeResponseHead("application/x-protobuf"),
                                     std::move(consumer), std::nullopt);
  ASSERT_TRUE(WaitFor([&] { return client->IsConnected(); }));

  // Any bytes push the watchdog out; a framed message makes the delivery
  // observable.
  task_environment_.FastForwardBy(stall_timeout / 2);
  std::string_view bytes = "still alive";
  size_t written = 0;
  ASSERT_EQ(producer->WriteData(base::as_byte_span(bytes),
                                MOJO_WRITE_DATA_FLAG_NONE, written),
            MOJO_RESULT_OK);
  ASSERT_TRUE(WaitFor([&] { return observer.messages.size() >= 1u; }));
  task_environment_.FastForwardBy(stall_timeout / 2);
  EXPECT_TRUE(client->IsConnected());

  // Full silence for the stall timeout: the stream is declared dead and
  // a reconnect is scheduled.
  task_environment_.FastForwardBy(stall_timeout);
  EXPECT_FALSE(client->IsConnected());
  ServeStream("back");
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_GE(requests_.size(), 2u);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, StallWatchdogDisabledByZeroTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kBrowserActuatorProtoStreamTransport,
      {{"ProtoStreamStallTimeout", "0s"}});

  auto client = MakeClient();
  client->Connect();
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

  // A live-but-silent stream, as in StallWatchdogReconnectsSilentStream.
  network::TestURLLoaderFactory::PendingRequest* pending =
      test_url_loader_factory_.GetPendingRequest(0);
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer, consumer), MOJO_RESULT_OK);
  pending->client->OnReceiveResponse(MakeResponseHead("application/x-protobuf"),
                                     std::move(consumer), std::nullopt);
  ASSERT_TRUE(WaitFor([&] { return client->IsConnected(); }));

  // With a zero stall timeout the watchdog never arms: total silence far
  // beyond the default timeout tears nothing down.
  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_TRUE(client->IsConnected());
  EXPECT_EQ(requests_.size(), 1u);
}

TEST_F(ProtoStreamClientTest, DisconnectCancelsPendingReconnect) {
  ServeStream("body");

  RecordingObserver observer;
  auto client = MakeClient();
  client->AddObserver(&observer);
  client->Connect();
  // Wait until the stream has ended, so that a reconnect is pending.
  ASSERT_TRUE(WaitFor([&] { return observer.state_changes.size() >= 2u; }));
  ASSERT_EQ(requests_.size(), 1u);

  client->Disconnect();
  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_EQ(requests_.size(), 1u);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, DelegatePreparesAsynchronously) {
  ServeStream("async body");

  std::unique_ptr<network::ResourceRequest> held_request;
  StreamConnectionDelegate::PrepareRequestCallback held_callback;
  auto delegate = std::make_unique<NiceMock<MockStreamConnectionDelegate>>();
  ON_CALL(*delegate, PrepareRequest)
      .WillByDefault(
          [&](std::unique_ptr<network::ResourceRequest> request,
              StreamConnectionDelegate::PrepareRequestCallback callback) {
            held_request = std::move(request);
            held_callback = std::move(callback);
          });

  RecordingObserver observer;
  auto client = MakeClient(std::move(delegate));
  client->AddObserver(&observer);
  client->Connect();

  // The attempt is parked inside the delegate: no request goes out, and
  // another Connect() must not start a competing attempt.
  task_environment_.FastForwardBy(base::Minutes(1));
  client->Connect();
  EXPECT_TRUE(requests_.empty());

  std::move(held_callback).Run(std::move(held_request));
  ASSERT_TRUE(WaitFor([&] { return !observer.messages.empty(); }));
  EXPECT_EQ(requests_.size(), 1u);

  client->RemoveObserver(&observer);
}

TEST_F(ProtoStreamClientTest, DisconnectCancelsPendingPrepare) {
  ServeStream("body");

  std::unique_ptr<network::ResourceRequest> held_request;
  StreamConnectionDelegate::PrepareRequestCallback held_callback;
  auto delegate = std::make_unique<NiceMock<MockStreamConnectionDelegate>>();
  ON_CALL(*delegate, PrepareRequest)
      .WillByDefault(
          [&](std::unique_ptr<network::ResourceRequest> request,
              StreamConnectionDelegate::PrepareRequestCallback callback) {
            held_request = std::move(request);
            held_callback = std::move(callback);
          });

  auto client = MakeClient(std::move(delegate));
  client->Connect();
  client->Disconnect();

  // The prepared request arrives after teardown: it must be dropped.
  std::move(held_callback).Run(std::move(held_request));
  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_TRUE(requests_.empty());
  EXPECT_FALSE(client->IsConnected());
}

// A delegate that offers a fixed upload body, to exercise the client's
// POST + AttachStringForUpload path.
class FixedBodyDelegate : public StreamConnectionDelegate {
 public:
  void PrepareRequest(std::unique_ptr<network::ResourceRequest> request,
                      PrepareRequestCallback callback) override {
    std::move(callback).Run(std::move(request));
  }
  std::optional<StreamUploadBody> GetConnectionRequestBody() override {
    return StreamUploadBody{"watch-request-bytes", "application/x-protobuf"};
  }
};

TEST_F(ProtoStreamClientTest, UploadsDelegateBodyAsPost) {
  ServeStream("stream body bytes");
  auto client = MakeClient(std::make_unique<FixedBodyDelegate>());
  client->Connect();
  ASSERT_TRUE(WaitFor([&] { return !requests_.empty(); }));

  EXPECT_EQ(requests_[0].method, "POST");
  EXPECT_EQ(network::GetUploadData(requests_[0]), "watch-request-bytes");
}

}  // namespace
}  // namespace browser_actuator

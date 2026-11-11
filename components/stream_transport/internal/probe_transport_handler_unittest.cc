// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/probe_transport_handler.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/stream_transport/internal/probe_payload.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/payload_type_mapping.h"
#include "components/stream_transport/public/transport_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace stream_transport {
namespace {

class MockTransportSession : public TransportSession {
 public:
  explicit MockTransportSession(std::string_view session_id)
      : session_id_(session_id) {}
  ~MockTransportSession() override = default;

  std::string_view GetSessionId() const override { return session_id_; }
  MOCK_METHOD((base::expected<void, SendUpstreamMessageError>),
              SendUpstreamMessage,
              (PayloadType payload_type,
               const google::protobuf::MessageLite& message),
              (override));
  MOCK_METHOD(void,
              OnMessage,
              (PayloadType payload_type, std::string_view serialized_payload),
              (override));
  MOCK_METHOD(void, Close, (), (override));

 private:
  std::string session_id_;
};

struct ProbeReplyCapture {
  struct CapturedProbeReply {
    std::string session_id;
    browser_actuator::TransportProbe probe;
    std::optional<int64_t> responding_to_sequence_number;
  };

  std::optional<int64_t> dispatching_sequence_number;
  std::vector<CapturedProbeReply> replies;

  ProbeTransportHandler::GetDispatchingSequenceCallback
  MakeGetSequenceCallback() {
    return base::BindLambdaForTesting(
        [this](std::string_view /*session_id*/) -> std::optional<int64_t> {
          return dispatching_sequence_number;
        });
  }

  ProbeTransportHandler::SendProbeReplyCallback MakeSendReplyCallback() {
    return base::BindLambdaForTesting(
        [this](std::string_view session_id,
               const browser_actuator::TransportProbe& probe,
               std::optional<int64_t> responding_to_sequence_number)
            -> base::expected<void, SendUpstreamMessageError> {
          replies.push_back(CapturedProbeReply{
              .session_id = std::string(session_id),
              .probe = probe,
              .responding_to_sequence_number = responding_to_sequence_number,
          });
          return {};
        });
  }
};

TEST(ProbeTransportHandlerTest, FactoryGetFactoryId) {
  ProbeTransportHandlerFactory factory;
  EXPECT_EQ(factory.GetFactoryId(), FactoryId::kProbe);
}

TEST(ProbeTransportHandlerTest, FactoryGetSupportedPayloadTypes) {
  ProbeTransportHandlerFactory factory;
  EXPECT_THAT(factory.GetSupportedPayloadTypes(),
              ::testing::ElementsAre(PayloadType::kProbe));
}

TEST(ProbeTransportHandlerTest, FactoryUnsupportedPayloadType) {
  ProbeTransportHandlerFactory factory;
  EXPECT_THAT(factory.GetSupportedPayloadTypes(),
              ::testing::Not(::testing::Contains(PayloadType::kUnspecified)));
}

TEST(ProbeTransportHandlerTest, FactoryOnNewSession) {
  ProbeTransportHandlerFactory factory;
  MockTransportSession session("session_abc");
  std::unique_ptr<TransportHandler> handler = factory.OnNewSession(&session);
  ASSERT_NE(handler, nullptr);

  EXPECT_CALL(session, SendUpstreamMessage(PayloadType::kProbe, testing::_))
      .Times(1);

  const ProbeResponseSpecInput specs[] = {
      {.payload_size_bytes = 32, .delay_ms = 0},
  };
  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      request = BuildProbeRequest("factory-probe",
                                  /*request_payload_size_bytes=*/0, specs);
  ASSERT_TRUE(request.has_value());

  browser_actuator::TransportProbe probe;
  *probe.mutable_request() = *std::move(request);
  handler->OnMessage(PayloadType::kProbe, probe.SerializeAsString());
}

// Pins `ExpectedTypeUrl()` to the generated proto; this layer may include it.
// LINT.IfChange(ExpectedTypeUrlMatchesTransportProbe)
TEST(ProbeTransportHandlerTest, ExpectedTypeUrlMatchesTransportProbe) {
  EXPECT_EQ(ExpectedTypeUrl(PayloadType::kProbe),
            base::StrCat({kTypeUrlPrefix,
                          browser_actuator::TransportProbe().GetTypeName()}));
}
// LINT.ThenChange(//components/stream_transport/public/payload_type_mapping.h:ExpectedTypeUrl)

TEST(ProbeTransportHandlerTest, MalformedPayloadIsIgnored) {
  MockTransportSession session("session_1");
  ProbeTransportHandler handler(&session);

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  // Field number 13 with wire type 6, which is not a valid wire type.
  handler.OnMessage(PayloadType::kProbe, "not a valid proto");
}

TEST(ProbeTransportHandlerTest, OnMessageUnsetPayloadIsIgnored) {
  MockTransportSession session("session_1");
  ProbeTransportHandler handler(&session);

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  browser_actuator::TransportProbe empty_probe;
  handler.OnMessage(PayloadType::kProbe, empty_probe.SerializeAsString());
}

TEST(ProbeTransportHandlerTest, OnMessageUnexpectedProbeResponseIsIgnored) {
  MockTransportSession session("session_1");
  ProbeTransportHandler handler(&session);

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  browser_actuator::TransportProbe response_probe;
  response_probe.mutable_response()->set_probe_id("p-1");
  handler.OnMessage(PayloadType::kProbe, response_probe.SerializeAsString());
}

TEST(ProbeTransportHandlerTest, ProbeRequestWithNullSessionNoops) {
  ProbeReplyCapture capture;
  ProbeTransportHandler handler(nullptr, capture.MakeGetSequenceCallback(),
                                capture.MakeSendReplyCallback());

  const ProbeResponseSpecInput specs[] = {
      {.payload_size_bytes = 32, .delay_ms = 0},
  };
  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      request = BuildProbeRequest("null-session",
                                  /*request_payload_size_bytes=*/0, specs);
  ASSERT_TRUE(request.has_value());

  browser_actuator::TransportProbe probe;
  *probe.mutable_request() = *std::move(request);
  handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
  EXPECT_TRUE(capture.replies.empty());
}

TEST(ProbeTransportHandlerTest, ProbeRequestWithZeroSpecsActsAsOneWaySink) {
  MockTransportSession session("session_sink");
  ProbeTransportHandler handler(&session);

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      request = BuildProbeRequest("sink-1", /*request_payload_size_bytes=*/256,
                                  /*response_specs=*/{});
  ASSERT_TRUE(request.has_value());

  browser_actuator::TransportProbe probe;
  *probe.mutable_request() = *std::move(request);
  handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
}

TEST(ProbeTransportHandlerTest,
     ProbeRequestEmitsMultiResponseFanOutWithDeterministicPayloads) {
  MockTransportSession session("session_probe");
  ProbeTransportHandler handler(&session);

  const ProbeResponseSpecInput specs[] = {
      {.payload_size_bytes = 0, .delay_ms = 0},
      {.payload_size_bytes = 16, .delay_ms = 0},
      {.payload_size_bytes = 64, .delay_ms = 0},
  };
  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      request =
          BuildProbeRequest("p-42", /*request_payload_size_bytes=*/64, specs);
  ASSERT_TRUE(request.has_value());

  std::vector<browser_actuator::TransportProbe::ProbeResponse> responses;
  EXPECT_CALL(session, SendUpstreamMessage(PayloadType::kProbe, testing::_))
      .Times(3)
      .WillRepeatedly([&responses](PayloadType /*payload_type*/,
                                   const google::protobuf::MessageLite& msg)
                          -> base::expected<void, SendUpstreamMessageError> {
        EXPECT_EQ(
            msg.GetTypeName(),
            browser_actuator::TransportProbe::default_instance().GetTypeName());
        const auto* probe_msg =
            static_cast<const browser_actuator::TransportProbe*>(&msg);
        EXPECT_TRUE(probe_msg->has_response());
        responses.push_back(probe_msg->response());
        return {};
      });

  browser_actuator::TransportProbe probe;
  *probe.mutable_request() = *request;
  handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());

  ASSERT_EQ(responses.size(), 3u);
  for (int32_t i = 0; i < 3; ++i) {
    EXPECT_EQ(responses[i].response_index(), i);
    EXPECT_EQ(responses[i].total_responses(), 3);
    EXPECT_TRUE(VerifyProbeResponsePayload(*request, responses[i]));
  }
}

TEST(
    ProbeTransportHandlerTest,
    ProbeRequestSchedulesDelayedResponsesAndPreservesDispatchingSequenceNumber) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockTransportSession session("session_delayed");
  ProbeReplyCapture capture;
  ProbeTransportHandler handler(&session, capture.MakeGetSequenceCallback(),
                                capture.MakeSendReplyCallback());

  const ProbeResponseSpecInput specs[] = {
      {.payload_size_bytes = 64, .delay_ms = 0},
      {.payload_size_bytes = 128, .delay_ms = 250},
      {.payload_size_bytes = 32, .delay_ms = 50},
  };
  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      request = BuildProbeRequest("delayed-probe",
                                  /*request_payload_size_bytes=*/0, specs);
  ASSERT_TRUE(request.has_value());

  browser_actuator::TransportProbe probe;
  *probe.mutable_request() = *request;

  capture.dispatching_sequence_number = 17;
  handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
  // Simulate `TransportSessionImpl` unwinding its synchronous dispatch frame.
  capture.dispatching_sequence_number.reset();

  ASSERT_EQ(capture.replies.size(), 1u);
  EXPECT_EQ(capture.replies[0].session_id, "session_delayed");
  EXPECT_EQ(capture.replies[0].responding_to_sequence_number, 17);
  EXPECT_EQ(capture.replies[0].probe.response().response_index(), 0);
  EXPECT_TRUE(VerifyProbeResponsePayload(*request,
                                         capture.replies[0].probe.response()));

  task_environment.FastForwardBy(base::Milliseconds(50));
  ASSERT_EQ(capture.replies.size(), 2u);
  EXPECT_EQ(capture.replies[1].responding_to_sequence_number, 17);
  EXPECT_EQ(capture.replies[1].probe.response().response_index(), 2);
  EXPECT_TRUE(VerifyProbeResponsePayload(*request,
                                         capture.replies[1].probe.response()));

  task_environment.FastForwardBy(base::Milliseconds(200));
  ASSERT_EQ(capture.replies.size(), 3u);
  EXPECT_EQ(capture.replies[2].responding_to_sequence_number, 17);
  EXPECT_EQ(capture.replies[2].probe.response().response_index(), 1);
  EXPECT_TRUE(VerifyProbeResponsePayload(*request,
                                         capture.replies[2].probe.response()));
}

TEST(ProbeTransportHandlerTest,
     DestroyingHandlerCancelsPendingDelayedProbeResponses) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockTransportSession session("session_closing");
  ProbeReplyCapture capture;
  auto handler = std::make_unique<ProbeTransportHandler>(
      &session, capture.MakeGetSequenceCallback(),
      capture.MakeSendReplyCallback());

  const ProbeResponseSpecInput specs[] = {
      {.payload_size_bytes = 1024, .delay_ms = 500},
  };
  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      request = BuildProbeRequest("cancel-probe",
                                  /*request_payload_size_bytes=*/0, specs);
  ASSERT_TRUE(request.has_value());

  browser_actuator::TransportProbe probe;
  *probe.mutable_request() = *std::move(request);
  handler->OnMessage(PayloadType::kProbe, probe.SerializeAsString());
  EXPECT_TRUE(capture.replies.empty());

  handler.reset();
  task_environment.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(capture.replies.empty());
}

TEST(
    ProbeTransportHandlerTest,
    OverlappingDelayedProbeRequestsExceedingSessionBudgetAreRejectedAtomically) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockTransportSession session("session_budget");
  ProbeReplyCapture capture;
  ProbeTransportHandler handler(&session, capture.MakeGetSequenceCallback(),
                                capture.MakeSendReplyCallback());

  // 1. Saturate the session's 20 MB delayed byte budget with two 10 MB delayed
  // responses (`delay_ms = 200`).
  const ProbeResponseSpecInput full_bytes_specs[] = {
      {.payload_size_bytes = kMaxProbePayloadBytes, .delay_ms = 200},
      {.payload_size_bytes = kMaxProbePayloadBytes, .delay_ms = 200},
  };
  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      first_request = BuildProbeRequest(
          "budget-bytes-1", /*request_payload_size_bytes=*/0, full_bytes_specs);
  ASSERT_TRUE(first_request.has_value());
  browser_actuator::TransportProbe first_probe;
  *first_probe.mutable_request() = *first_request;
  handler.OnMessage(PayloadType::kProbe, first_probe.SerializeAsString());
  EXPECT_TRUE(capture.replies.empty());

  // 2. An overlapping `ProbeRequest` mixing an immediate (`delay_ms = 0`) reply
  // and a 1-byte delayed reply exceeds `kMaxTotalProbeResponseBytes` and must
  // be rejected atomically without emitting even the immediate reply.
  const ProbeResponseSpecInput over_bytes_specs[] = {
      {.payload_size_bytes = 64, .delay_ms = 0},
      {.payload_size_bytes = 1, .delay_ms = 50},
  };
  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      over_bytes_request =
          BuildProbeRequest("budget-bytes-over",
                            /*request_payload_size_bytes=*/0, over_bytes_specs);
  ASSERT_TRUE(over_bytes_request.has_value());
  browser_actuator::TransportProbe over_bytes_probe;
  *over_bytes_probe.mutable_request() = *over_bytes_request;
  handler.OnMessage(PayloadType::kProbe, over_bytes_probe.SerializeAsString());
  EXPECT_TRUE(capture.replies.empty());

  // 3. Once the first probe's delayed timers fire, the session budget is
  // released and subsequent delayed probes are accepted again.
  task_environment.FastForwardBy(base::Milliseconds(200));
  ASSERT_EQ(capture.replies.size(), 2u);
  capture.replies.clear();

  // 4. Saturate `kMaxProbeResponses` (16) zero-byte delayed responses and
  // verify a 17th overlapping delayed response is rejected atomically.
  std::vector<ProbeResponseSpecInput> max_count_specs(
      kMaxProbeResponses, {.payload_size_bytes = 0, .delay_ms = 100});
  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      max_count_request = BuildProbeRequest(
          "budget-count-16", /*request_payload_size_bytes=*/0, max_count_specs);
  ASSERT_TRUE(max_count_request.has_value());
  browser_actuator::TransportProbe max_count_probe;
  *max_count_probe.mutable_request() = *max_count_request;
  handler.OnMessage(PayloadType::kProbe, max_count_probe.SerializeAsString());

  const ProbeResponseSpecInput extra_delayed_specs[] = {
      {.payload_size_bytes = 16, .delay_ms = 0},
      {.payload_size_bytes = 0, .delay_ms = 50},
  };
  base::expected<browser_actuator::TransportProbe::ProbeRequest, std::string>
      over_count_request = BuildProbeRequest("budget-count-over",
                                             /*request_payload_size_bytes=*/0,
                                             extra_delayed_specs);
  ASSERT_TRUE(over_count_request.has_value());
  browser_actuator::TransportProbe over_count_probe;
  *over_count_probe.mutable_request() = *over_count_request;
  handler.OnMessage(PayloadType::kProbe, over_count_probe.SerializeAsString());
  EXPECT_TRUE(capture.replies.empty());

  task_environment.FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(capture.replies.size(), static_cast<size_t>(kMaxProbeResponses));
}

TEST(ProbeTransportHandlerTest, InvalidProbeRequestExceedingBoundsIsDropped) {
  MockTransportSession session("session_invalid");
  ProbeTransportHandler handler(&session);

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  // 1. Single response exceeding `kMaxProbePayloadBytes`.
  {
    browser_actuator::TransportProbe probe;
    browser_actuator::TransportProbe::ProbeRequest* req =
        probe.mutable_request();
    req->set_probe_id("oversize-single");
    req->add_response_specs()->set_payload_size_bytes(kMaxProbePayloadBytes +
                                                      1);
    handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
  }

  // 2. Aggregate response bytes exceeding `kMaxTotalProbeResponseBytes`.
  {
    browser_actuator::TransportProbe probe;
    browser_actuator::TransportProbe::ProbeRequest* req =
        probe.mutable_request();
    req->set_probe_id("oversize-total");
    req->add_response_specs()->set_payload_size_bytes(kMaxProbePayloadBytes);
    req->add_response_specs()->set_payload_size_bytes(kMaxProbePayloadBytes);
    req->add_response_specs()->set_payload_size_bytes(1);
    handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
  }

  // 3. Delay exceeding `kMaxProbeDelayMs`.
  {
    browser_actuator::TransportProbe probe;
    browser_actuator::TransportProbe::ProbeRequest* req =
        probe.mutable_request();
    req->set_probe_id("oversize-delay");
    browser_actuator::TransportProbe::ProbeRequest::ResponseSpec* spec =
        req->add_response_specs();
    spec->set_payload_size_bytes(64);
    spec->set_delay_ms(kMaxProbeDelayMs + 1);
    handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
  }
}

}  // namespace
}  // namespace stream_transport

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/probe_transport_handler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/transport/probe_payload.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/public/transport/transport_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator::transport {
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
  base::expected<TransportProbe::ProbeRequest, std::string> request =
      BuildProbeRequest("factory-probe", /*request_payload_size_bytes=*/0,
                        specs);
  ASSERT_TRUE(request.has_value());

  TransportProbe probe;
  *probe.mutable_request() = *std::move(request);
  handler->OnMessage(PayloadType::kProbe, probe.SerializeAsString());
}

// Pins `ExpectedTypeUrl()` to the generated proto; this layer may include it.
// LINT.IfChange(ExpectedTypeUrlMatchesTransportProbe)
TEST(ProbeTransportHandlerTest, ExpectedTypeUrlMatchesTransportProbe) {
  EXPECT_EQ(ExpectedTypeUrl(PayloadType::kProbe),
            base::StrCat({kTypeUrlPrefix, TransportProbe().GetTypeName()}));
}
// LINT.ThenChange(//components/browser_actuator/public/transport/payload_type_mapping.h:ExpectedTypeUrl)

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

  TransportProbe empty_probe;
  handler.OnMessage(PayloadType::kProbe, empty_probe.SerializeAsString());
}

TEST(ProbeTransportHandlerTest, OnMessageUnexpectedProbeResponseIsIgnored) {
  MockTransportSession session("session_1");
  ProbeTransportHandler handler(&session);

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  TransportProbe response_probe;
  response_probe.mutable_response()->set_probe_id("p-1");
  handler.OnMessage(PayloadType::kProbe, response_probe.SerializeAsString());
}

TEST(ProbeTransportHandlerTest, ProbeRequestWithNullSessionNoops) {
  ProbeTransportHandler handler(nullptr);

  const ProbeResponseSpecInput specs[] = {
      {.payload_size_bytes = 32, .delay_ms = 0},
  };
  base::expected<TransportProbe::ProbeRequest, std::string> request =
      BuildProbeRequest("null-session", /*request_payload_size_bytes=*/0,
                        specs);
  ASSERT_TRUE(request.has_value());

  TransportProbe probe;
  *probe.mutable_request() = *std::move(request);
  handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
}

TEST(ProbeTransportHandlerTest, ProbeRequestWithZeroSpecsActsAsOneWaySink) {
  MockTransportSession session("session_sink");
  ProbeTransportHandler handler(&session);

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  base::expected<TransportProbe::ProbeRequest, std::string> request =
      BuildProbeRequest("sink-1", /*request_payload_size_bytes=*/256,
                        /*response_specs=*/{});
  ASSERT_TRUE(request.has_value());

  TransportProbe probe;
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
  base::expected<TransportProbe::ProbeRequest, std::string> request =
      BuildProbeRequest("p-42", /*request_payload_size_bytes=*/64, specs);
  ASSERT_TRUE(request.has_value());

  std::vector<TransportProbe::ProbeResponse> responses;
  EXPECT_CALL(session, SendUpstreamMessage(PayloadType::kProbe, testing::_))
      .Times(3)
      .WillRepeatedly([&responses](PayloadType /*payload_type*/,
                                   const google::protobuf::MessageLite& msg)
                          -> base::expected<void, SendUpstreamMessageError> {
        EXPECT_EQ(msg.GetTypeName(),
                  TransportProbe::default_instance().GetTypeName());
        const auto* probe_msg = static_cast<const TransportProbe*>(&msg);
        EXPECT_TRUE(probe_msg->has_response());
        responses.push_back(probe_msg->response());
        return {};
      });

  TransportProbe probe;
  *probe.mutable_request() = *request;
  handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());

  ASSERT_EQ(responses.size(), 3u);
  for (int32_t i = 0; i < 3; ++i) {
    EXPECT_EQ(responses[i].response_index(), i);
    EXPECT_EQ(responses[i].total_responses(), 3);
    EXPECT_TRUE(VerifyProbeResponsePayload(*request, responses[i]));
  }
}

TEST(ProbeTransportHandlerTest,
     DelayedProbeRequestIsDroppedWithoutEmittingPartialResponses) {
  MockTransportSession session("session_delayed_unsupported");
  ProbeTransportHandler handler(&session);

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  const ProbeResponseSpecInput specs[] = {
      {.payload_size_bytes = 64, .delay_ms = 0},
      {.payload_size_bytes = 64, .delay_ms = 50},
  };
  base::expected<TransportProbe::ProbeRequest, std::string> request =
      BuildProbeRequest("delayed-unsupported", /*request_payload_size_bytes=*/0,
                        specs);
  ASSERT_TRUE(request.has_value());

  TransportProbe probe;
  *probe.mutable_request() = *std::move(request);
  handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
}

TEST(ProbeTransportHandlerTest, InvalidProbeRequestExceedingBoundsIsDropped) {
  MockTransportSession session("session_invalid");
  ProbeTransportHandler handler(&session);

  EXPECT_CALL(session, SendUpstreamMessage(testing::_, testing::_)).Times(0);

  // 1. Single response exceeding `kMaxProbePayloadBytes`.
  {
    TransportProbe probe;
    TransportProbe::ProbeRequest* req = probe.mutable_request();
    req->set_probe_id("oversize-single");
    req->add_response_specs()->set_payload_size_bytes(kMaxProbePayloadBytes +
                                                      1);
    handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
  }

  // 2. Aggregate response bytes exceeding `kMaxTotalProbeResponseBytes`.
  {
    TransportProbe probe;
    TransportProbe::ProbeRequest* req = probe.mutable_request();
    req->set_probe_id("oversize-total");
    req->add_response_specs()->set_payload_size_bytes(kMaxProbePayloadBytes);
    req->add_response_specs()->set_payload_size_bytes(kMaxProbePayloadBytes);
    req->add_response_specs()->set_payload_size_bytes(1);
    handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
  }

  // 3. Delay exceeding `kMaxProbeDelayMs`.
  {
    TransportProbe probe;
    TransportProbe::ProbeRequest* req = probe.mutable_request();
    req->set_probe_id("oversize-delay");
    TransportProbe::ProbeRequest::ResponseSpec* spec =
        req->add_response_specs();
    spec->set_payload_size_bytes(64);
    spec->set_delay_ms(kMaxProbeDelayMs + 1);
    handler.OnMessage(PayloadType::kProbe, probe.SerializeAsString());
  }
}

}  // namespace
}  // namespace browser_actuator::transport

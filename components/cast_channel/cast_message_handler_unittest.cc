// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/values_test_util.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJson;
using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::WithArg;

namespace cast_channel {

namespace {

constexpr char kTestUserAgentString[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/66.0.3331.0 Safari/537.36";
constexpr char kSourceId[] = "sourceId";
constexpr char kDestinationId[] = "destinationId";

data_decoder::DataDecoder::ValueOrError ParseJsonLikeDataDecoder(
    base::StringPiece json) {
  return data_decoder::DataDecoder::ValueOrError::Value(ParseJson(json));
}

std::unique_ptr<base::Value> GetDictionaryFromCastMessage(
    const CastMessage& message) {
  if (!message.has_payload_utf8())
    return nullptr;

  return base::JSONReader::ReadDeprecated(message.payload_utf8());
}

CastMessageType GetMessageType(const CastMessage& message) {
  std::unique_ptr<base::Value> dict = GetDictionaryFromCastMessage(message);
  if (!dict)
    return CastMessageType::kOther;

  const base::Value* message_type =
      dict->FindKeyOfType("type", base::Value::Type::STRING);
  if (!message_type)
    return CastMessageType::kOther;

  return CastMessageTypeFromString(message_type->GetString());
}

MATCHER_P(HasMessageType, type, "") {
  return GetMessageType(arg) == type;
}

MATCHER_P(HasPayloadUtf8, payload, "") {
  return arg.payload_utf8() == payload;
}

}  // namespace

class CastMessageHandlerTest : public testing::Test {
 public:
  CastMessageHandlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        cast_socket_service_(new base::TestSimpleTaskRunner()),
        handler_(
            &cast_socket_service_,
            base::BindRepeating(&data_decoder::DataDecoder::ParseJsonIsolated),
            kTestUserAgentString,
            "66.0.3331.0",
            "en-US") {
    ON_CALL(cast_socket_service_, GetSocket(_))
        .WillByDefault(testing::Return(&cast_socket_));
  }

  ~CastMessageHandlerTest() override {}

  void OnMessage(const CastMessage& message) {
    handler_.OnMessage(cast_socket_, message);
  }

  void OnError(ChannelError error) { handler_.OnError(cast_socket_, error); }

  void OnAppAvailability(const std::string& app_id,
                         GetAppAvailabilityResult result) {
    if (run_loop_)
      run_loop_->Quit();
    DoOnAppAvailability(app_id, result);
  }

  MOCK_METHOD2(DoOnAppAvailability,
               void(const std::string& app_id,
                    GetAppAvailabilityResult result));

  void ExpectSessionLaunchResult(LaunchSessionResponse::Result expected_result,
                                 LaunchSessionResponse response) {
    if (run_loop_)
      run_loop_->Quit();
    ++session_launch_response_count_;
    EXPECT_EQ(expected_result, response.result);
    if (response.result == LaunchSessionResponse::Result::kOk)
      EXPECT_TRUE(response.receiver_status);
  }

  void ExpectEnsureConnection() {
    EXPECT_CALL(*transport_,
                SendMessage(HasMessageType(CastMessageType::kConnect), _));
  }

  void ExpectEnsureConnectionThen(CastMessageType next_type,
                                  int request_count = 1) {
    InSequence dummy;
    ExpectEnsureConnection();
    EXPECT_CALL(*transport_, SendMessage(HasMessageType(next_type), _))
        .Times(request_count)
        .WillRepeatedly(SaveArg<0>(&last_request_));
  }

  void CreatePendingRequests() {
    EXPECT_CALL(*transport_, SendMessage(_, _)).Times(AnyNumber());
    handler_.LaunchSession(channel_id_, "theAppId", base::TimeDelta::Max(),
                           launch_session_callback_.Get());
    for (int i = 0; i < 2; i++) {
      handler_.RequestAppAvailability(&cast_socket_, "theAppId",
                                      get_app_availability_callback_.Get());
      handler_.SendSetVolumeRequest(
          channel_id_,
          ParseJson(R"({"sessionId": "theSessionId", "type": "SET_VOLUME"})"),
          "theSourceId", set_volume_callback_.Get());
    }
    handler_.StopSession(channel_id_, "theSessionId", "theSourceId",
                         stop_session_callback_.Get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  testing::NiceMock<MockCastSocketService> cast_socket_service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  CastMessageHandler handler_;
  MockCastSocket cast_socket_;
  const int channel_id_ = cast_socket_.id();
  MockCastTransport* const transport_ = cast_socket_.mock_transport();
  int session_launch_response_count_ = 0;
  CastMessage last_request_;
  base::MockCallback<LaunchSessionCallback> launch_session_callback_;
  base::MockCallback<GetAppAvailabilityCallback> get_app_availability_callback_;
  base::MockCallback<ResultCallback> set_volume_callback_;
  base::MockCallback<ResultCallback> stop_session_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastMessageHandlerTest);
};

TEST_F(CastMessageHandlerTest, VirtualConnectionCreatedOnlyOnce) {
  ExpectEnsureConnectionThen(CastMessageType::kGetAppAvailability, 2);

  handler_.RequestAppAvailability(
      &cast_socket_, "AAAAAAAA",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
  handler_.RequestAppAvailability(
      &cast_socket_, "BBBBBBBB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
}

TEST_F(CastMessageHandlerTest, RecreateVirtualConnectionAfterError) {
  ExpectEnsureConnectionThen(CastMessageType::kGetAppAvailability);

  handler_.RequestAppAvailability(
      &cast_socket_, "AAAAAAAA",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  EXPECT_CALL(*this, DoOnAppAvailability("AAAAAAAA",
                                         GetAppAvailabilityResult::kUnknown));
  OnError(ChannelError::TRANSPORT_ERROR);

  ExpectEnsureConnectionThen(CastMessageType::kGetAppAvailability);

  handler_.RequestAppAvailability(
      &cast_socket_, "BBBBBBBB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  // The callback is invoked with kUnknown before the PendingRequests is
  // destroyed.
  EXPECT_CALL(*this, DoOnAppAvailability("BBBBBBBB",
                                         GetAppAvailabilityResult::kUnknown));
}

TEST_F(CastMessageHandlerTest, RequestAppAvailability) {
  ExpectEnsureConnectionThen(CastMessageType::kGetAppAvailability);

  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  std::unique_ptr<base::Value> dict =
      GetDictionaryFromCastMessage(last_request_);
  ASSERT_TRUE(dict);
  const base::Value* request_id_value =
      dict->FindKeyOfType("requestId", base::Value::Type::INTEGER);
  ASSERT_TRUE(request_id_value);
  int request_id = request_id_value->GetInt();
  EXPECT_GT(request_id, 0);

  CastMessage response;
  response.set_namespace_("urn:x-cast:com.google.cast.receiver");
  response.set_source_id("receiver-0");
  response.set_destination_id(handler_.sender_id());
  response.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  response.set_payload_utf8(
      base::StringPrintf("{\"requestId\": %d, \"availability\": {\"ABCDEFAB\": "
                         "\"APP_AVAILABLE\"}}",
                         request_id));

  run_loop_ = std::make_unique<base::RunLoop>();
  EXPECT_CALL(*this, DoOnAppAvailability("ABCDEFAB",
                                         GetAppAvailabilityResult::kAvailable));
  OnMessage(response);
  run_loop_->Run();
}

TEST_F(CastMessageHandlerTest, RequestAppAvailabilityTimesOut) {
  EXPECT_CALL(*transport_, SendMessage(_, _)).Times(2);
  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
  EXPECT_CALL(*this, DoOnAppAvailability("ABCDEFAB",
                                         GetAppAvailabilityResult::kUnknown));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));
}

TEST_F(CastMessageHandlerTest, AppAvailabilitySentOnlyOnceWhilePending) {
  EXPECT_CALL(*transport_, SendMessage(_, _)).Times(2);
  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  EXPECT_CALL(*transport_, SendMessage(_, _)).Times(0);
  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
}

TEST_F(CastMessageHandlerTest, EnsureConnection) {
  ExpectEnsureConnection();

  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId);

  // No-op because connection is already created the first time.
  EXPECT_CALL(*transport_, SendMessage(_, _)).Times(0);
  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId);
}

TEST_F(CastMessageHandlerTest, CloseConnectionFromReceiver) {
  ExpectEnsureConnection();
  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId);

  CastMessage response;
  response.set_namespace_("urn:x-cast:com.google.cast.tp.connection");
  response.set_source_id(kDestinationId);
  response.set_destination_id(kSourceId);
  response.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  response.set_payload_utf8(R"({
      "type": "CLOSE"
  })");
  OnMessage(response);
  // Wait for message to be parsed and handled.
  task_environment_.RunUntilIdle();

  // Re-open virtual connection should cause message to be sent.
  EXPECT_CALL(*transport_, SendMessage(_, _));
  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId);
}

TEST_F(CastMessageHandlerTest, LaunchSession) {
  ExpectEnsureConnectionThen(CastMessageType::kLaunch);

  handler_.LaunchSession(
      channel_id_, "AAAAAAAA", base::TimeDelta::FromSeconds(30),
      base::BindOnce(&CastMessageHandlerTest::ExpectSessionLaunchResult,
                     base::Unretained(this),
                     LaunchSessionResponse::Result::kOk));

  std::unique_ptr<base::Value> dict =
      GetDictionaryFromCastMessage(last_request_);
  ASSERT_TRUE(dict);
  const base::Value* request_id_value =
      dict->FindKeyOfType("requestId", base::Value::Type::INTEGER);
  ASSERT_TRUE(request_id_value);
  int request_id = request_id_value->GetInt();
  EXPECT_GT(request_id, 0);

  CastMessage response;
  response.set_namespace_("urn:x-cast:com.google.cast.receiver");
  response.set_source_id("receiver-0");
  response.set_destination_id(handler_.sender_id());
  response.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);
  response.set_payload_utf8(
      base::StringPrintf("{"
                         "\"type\": \"RECEIVER_STATUS\","
                         "\"requestId\": %d,"
                         "\"status\": {}"
                         "}",
                         request_id));

  run_loop_ = std::make_unique<base::RunLoop>();
  OnMessage(response);
  run_loop_->Run();
  EXPECT_EQ(1, session_launch_response_count_);
}

TEST_F(CastMessageHandlerTest, LaunchSessionTimedOut) {
  ExpectEnsureConnectionThen(CastMessageType::kLaunch);

  handler_.LaunchSession(
      channel_id_, "AAAAAAAA", base::TimeDelta::FromSeconds(30),
      base::BindOnce(&CastMessageHandlerTest::ExpectSessionLaunchResult,
                     base::Unretained(this),
                     LaunchSessionResponse::Result::kTimedOut));

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(30));
  EXPECT_EQ(1, session_launch_response_count_);
}

TEST_F(CastMessageHandlerTest, SendAppMessage) {
  base::Value body(base::Value::Type::DICTIONARY);
  body.SetKey("foo", base::Value("bar"));
  CastMessage message =
      CreateCastMessage("namespace", body, kSourceId, kDestinationId);
  {
    InSequence dummy;
    ExpectEnsureConnection();
    EXPECT_CALL(*transport_,
                SendMessage(HasPayloadUtf8(message.payload_utf8()), _));
  }

  EXPECT_EQ(Result::kOk, handler_.SendAppMessage(channel_id_, message));
}

// Check that SendMediaRequest sends a message created by CreateMediaRequest and
// returns a request ID.
TEST_F(CastMessageHandlerTest, SendMediaRequest) {
  {
    InSequence dummy;
    ExpectEnsureConnection();
    EXPECT_CALL(*transport_, SendMessage(_, _))
        .WillOnce(WithArg<0>([&](const auto& message) {
          std::string expected_body = R"({
            "requestId": 1,
            "type": "PLAY",
          })";
          auto expected = CreateMediaRequest(ParseJson(expected_body), 1,
                                             "theSourceId", "theDestinationId");
          EXPECT_EQ(expected.namespace_(), message.namespace_());
          EXPECT_EQ(expected.source_id(), message.source_id());
          EXPECT_EQ(expected.destination_id(), message.destination_id());
          EXPECT_EQ(expected.payload_utf8(), message.payload_utf8());

          // Future-proofing. This matcher gives terrible error messages but it
          // might catch errors the above matchers miss.
          EXPECT_THAT(message, EqualsProto(expected));
        }));
  }

  std::string message_str = R"({
    "type": "PLAY",
  })";
  base::Optional<int> request_id = handler_.SendMediaRequest(
      channel_id_, ParseJson(message_str), "theSourceId", "theDestinationId");
  EXPECT_EQ(1, request_id);
}

// Check that SendVolumeCommand sends a message created by CreateVolumeRequest
// and registers a pending request.
TEST_F(CastMessageHandlerTest, SendVolumeCommand) {
  {
    InSequence dummy;
    ExpectEnsureConnection();
    EXPECT_CALL(*transport_, SendMessage(_, _))
        .WillOnce(WithArg<0>([&](const auto& message) {
          std::string expected_body = R"({
            "requestId": 1,
            "type": "SET_VOLUME",
          })";
          auto expected = CreateSetVolumeRequest(ParseJson(expected_body), 1,
                                                 "theSourceId");
          EXPECT_EQ(expected.namespace_(), message.namespace_());
          EXPECT_EQ(expected.source_id(), message.source_id());
          EXPECT_EQ(expected.destination_id(), message.destination_id());
          EXPECT_EQ(expected.payload_utf8(), message.payload_utf8());

          // Future-proofing. This matcher gives terrible error messages but it
          // might catch errors the above matchers miss.
          EXPECT_THAT(message, EqualsProto(expected));
        }));
  }

  std::string message_str = R"({
    "sessionId": "theSessionId",
    "type": "SET_VOLUME",
  })";
  handler_.SendSetVolumeRequest(channel_id_, ParseJson(message_str),
                                "theSourceId", base::DoNothing::Once<Result>());
}

// Check that closing a socket removes pending requests, and that the pending
// request callbacks are called appropriately.
TEST_F(CastMessageHandlerTest, PendingRequestsDestructor) {
  CreatePendingRequests();

  // Set up expanctions for pending request callbacks.
  EXPECT_CALL(launch_session_callback_, Run(_))
      .WillOnce([&](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::kError, response.result);
        EXPECT_EQ(base::nullopt, response.receiver_status);
      });
  EXPECT_CALL(get_app_availability_callback_,
              Run("theAppId", GetAppAvailabilityResult::kUnknown))
      .Times(2);
  EXPECT_CALL(set_volume_callback_, Run(Result::kFailed)).Times(2);
  EXPECT_CALL(stop_session_callback_, Run(Result::kFailed));

  // Force callbacks to be called through PendingRequests destructor by
  // simulating a socket closing.
  EXPECT_CALL(cast_socket_, ready_state()).WillOnce(Return(ReadyState::CLOSED));
  handler_.OnReadyStateChanged(cast_socket_);
}

TEST_F(CastMessageHandlerTest, HandlePendingRequest) {
  CreatePendingRequests();

  // Set up expanctions for pending request callbacks.
  EXPECT_CALL(launch_session_callback_, Run(_))
      .WillOnce([&](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::kOk, response.result);
        EXPECT_THAT(response.receiver_status,
                    testing::Optional(IsJson(R"({"foo": "bar"})")));
      });
  EXPECT_CALL(get_app_availability_callback_,
              Run("theAppId", GetAppAvailabilityResult::kAvailable))
      .Times(2);
  EXPECT_CALL(set_volume_callback_, Run(Result::kOk)).Times(2);
  EXPECT_CALL(stop_session_callback_, Run(Result::kOk));

  // Handle pending launch session request.
  handler_.HandleCastInternalMessage(channel_id_, "theSourceId",
                                     "theDestinationId", "theNamespace",
                                     ParseJsonLikeDataDecoder(R"(
      {
        "requestId": 1,
        "type": "RECEIVER_STATUS",
        "status": {"foo": "bar"},
      })"));

  // Handle both pending get app availability requests.
  handler_.HandleCastInternalMessage(channel_id_, "theSourceId",
                                     "theDestinationId", "theNamespace",
                                     ParseJsonLikeDataDecoder(R"(
      {
        "requestId": 2,
        "availability": {"theAppId": "APP_AVAILABLE"},
      })"));

  // Handle pending set volume request (1 of 2).
  handler_.HandleCastInternalMessage(
      channel_id_, "theSourceId", "theDestinationId", "theNamespace",
      ParseJsonLikeDataDecoder(R"({"requestId": 3})"));

  // Skip request_id == 4, since it was used by the second get app availability
  // request.

  // Handle pending set volume request (2 of 2).
  handler_.HandleCastInternalMessage(
      channel_id_, "theSourceId", "theDestinationId", "theNamespace",
      ParseJsonLikeDataDecoder(R"({"requestId": 5})"));

  // Handle pending stop session request.
  handler_.HandleCastInternalMessage(
      channel_id_, "theSourceId", "theDestinationId", "theNamespace",
      ParseJsonLikeDataDecoder(R"({"requestId": 6})"));
}

// Check that set volume requests time out correctly.
TEST_F(CastMessageHandlerTest, SetVolumeTimedOut) {
  EXPECT_CALL(*transport_, SendMessage(_, _)).Times(AnyNumber());

  std::string message_str = R"({
    "sessionId": "theSessionId",
    "type": "SET_VOLUME",
  })";
  base::MockCallback<ResultCallback> callback;
  handler_.SendSetVolumeRequest(channel_id_, ParseJson(message_str),
                                "theSourceId", callback.Get());
  EXPECT_CALL(callback, Run(Result::kFailed));
  task_environment_.FastForwardBy(kRequestTimeout);
}

}  // namespace cast_channel

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/values_test_util.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_metrics.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "components/media_router/common/providers/cast/channel/cast_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using base::test::ParseJson;
using base::test::ParseJsonDict;
using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::WithArg;

namespace cast_channel {

namespace {

constexpr char kAppId1[] = "0F5096E8";
constexpr char kAppId2[] = "85CDB22F";
constexpr char kTestUserAgentString[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/66.0.3331.0 Safari/537.36";
constexpr char kSessionId[] = "theSessionId";
constexpr char kSourceId[] = "theSourceId";
constexpr char kDestinationId[] = "theDestinationId";
constexpr char kAppParams[] = R"(
{
  "requiredFeatures" : ["STREAM_TRANSFER"],
  "launchCheckerParams" : {
    "credentialsData" : {
      "credentialsType" : "mobile",
      "credentials" : "99843n2idsguyhga"
    }
  }
}
)";
constexpr int kMaxProtocolMessageSize = 64 * 1024;

data_decoder::DataDecoder::ValueOrError ParseJsonLikeDataDecoder(
    std::string_view json) {
  return ParseJson(json);
}

std::optional<base::Value::Dict> GetDictionaryFromCastMessage(
    const CastMessage& message) {
  if (!message.has_payload_utf8())
    return std::nullopt;

  std::optional<base::Value> value =
      base::JSONReader::Read(message.payload_utf8());
  if (!value || !value->is_dict())
    return std::nullopt;
  return std::move(*value).TakeDict();
}

CastMessageType GetMessageType(const CastMessage& message) {
  std::optional<base::Value::Dict> dict = GetDictionaryFromCastMessage(message);
  if (!dict)
    return CastMessageType::kOther;

  const std::string* message_type = dict->FindString("type");
  if (!message_type)
    return CastMessageType::kOther;

  return CastMessageTypeFromString(*message_type);
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
    ON_CALL(cast_socket_service_, GetSocket(testing::Matcher<int>(_)))
        .WillByDefault(testing::Return(&cast_socket_));
  }

  CastMessageHandlerTest(const CastMessageHandlerTest&) = delete;
  CastMessageHandlerTest& operator=(const CastMessageHandlerTest&) = delete;

  ~CastMessageHandlerTest() override = default;

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
                                 LaunchSessionResponse response,
                                 LaunchSessionCallbackWrapper* out_callback) {
    if (run_loop_)
      run_loop_->Quit();
    ++session_launch_response_count_;
    EXPECT_EQ(expected_result, response.result);
    if (response.result == LaunchSessionResponse::Result::kOk)
      EXPECT_TRUE(response.receiver_status);
  }

  void ExpectEnsureConnection() {
    EXPECT_CALL(*transport_,
                SendMessage_(HasMessageType(CastMessageType::kConnect), _));
  }

  void ExpectEnsureConnectionThen(CastMessageType next_type,
                                  int request_count = 1) {
    InSequence dummy;
    ExpectEnsureConnection();
    EXPECT_CALL(*transport_, SendMessage_(HasMessageType(next_type), _))
        .Times(request_count)
        .WillRepeatedly(SaveArg<0>(&last_request_));
  }

  void CreatePendingRequests() {
    EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(AnyNumber());
    handler_.LaunchSession(channel_id_, kAppId1, base::TimeDelta::Max(),
                           {"WEB"}, /* appParams */ std::nullopt,
                           launch_session_callback_.Get());
    for (int i = 0; i < 2; i++) {
      handler_.RequestAppAvailability(&cast_socket_, kAppId1,
                                      get_app_availability_callback_.Get());
      handler_.SendSetVolumeRequest(
          channel_id_,
          ParseJsonDict(
              R"({"sessionId": "theSessionId", "type": "SET_VOLUME"})"),
          kSourceId, set_volume_callback_.Get());
    }
    handler_.StopSession(channel_id_, kSessionId, kSourceId,
                         stop_session_callback_.Get());
  }

  void SendMessageAndExpectConnection(const std::string& destination_id,
                                      VirtualConnectionType connection_type) {
    CastMessage message =
        CreateCastMessage("namespace", base::Value(base::Value::Dict()),
                          kSourceId, destination_id);
    {
      InSequence dummy;
      // We should first send a CONNECT request to ensure a connection.
      EXPECT_CALL(*transport_,
                  SendMessage_(HasMessageType(CastMessageType::kConnect), _))
          .WillOnce(WithArg<0>([&](const CastMessage& message) {
            std::optional<base::Value::Dict> dict =
                GetDictionaryFromCastMessage(message);
            EXPECT_EQ(connection_type, dict->FindInt("connType").value());
          }));
      // Then we send the actual message.
      EXPECT_CALL(*transport_, SendMessage_(_, _));
    }
    EXPECT_EQ(Result::kOk, handler_.SendAppMessage(channel_id_, message));
  }

  void HandlePendingLaunchSessionRequest(int request_id) {
    handler_.HandleCastInternalMessage(channel_id_, kSourceId, kDestinationId,
                                       "theNamespace",
                                       ParseJsonLikeDataDecoder(R"(
      {
        "requestId": )" + base::NumberToString(request_id) + R"(,
        "type": "RECEIVER_STATUS",
        "status": {"foo": "bar"},
      })"));
  }

  void HandleLaunchStatusResponse(int request_id, std::string message_status) {
    handler_.HandleCastInternalMessage(
        channel_id_,
        kSourceId, kDestinationId, "theNamespace", ParseJsonLikeDataDecoder(R"(
      {
        "launchRequestId": )" + base::NumberToString(request_id) + R"(,
        "type": "LAUNCH_STATUS",
        "status": ")" + message_status + R"(",
      })"));
  }

  void HandleLaunchErrorResponse(int request_id, std::string extended_error) {
    handler_.HandleCastInternalMessage(channel_id_, kSourceId, kDestinationId,
                                       "theNamespace",
                                       ParseJsonLikeDataDecoder(R"(
      {
        "requestId": )" + base::NumberToString(request_id) + R"(,
        "type": "LAUNCH_ERROR",
        "extendedError": ")" + extended_error + R"(",
      })"));
  }

  void HandlePendingGeneralRequest(int request_id) {
    handler_.HandleCastInternalMessage(channel_id_, kSourceId, kDestinationId,
                                       "theNamespace",
                                       ParseJsonLikeDataDecoder(R"(
      {
        "requestId": )" + base::NumberToString(request_id) + R"(
      })"));
  }

  void HandleAppAvailabilityRequest(int request_id) {
    handler_.HandleCastInternalMessage(channel_id_, kSourceId, kDestinationId,
                                       "theNamespace",
                                       ParseJsonLikeDataDecoder(R"(
      {
        "requestId": )" + base::NumberToString(request_id) + R"(,
        "availability": {")" + kAppId1 + R"(": "APP_AVAILABLE"},
      })"));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  testing::NiceMock<MockCastSocketService> cast_socket_service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  CastMessageHandler handler_;
  MockCastSocket cast_socket_;
  const int channel_id_ = cast_socket_.id();
  const raw_ptr<MockCastTransport> transport_ = cast_socket_.mock_transport();
  int session_launch_response_count_ = 0;
  CastMessage last_request_;
  base::MockCallback<LaunchSessionCallback> launch_session_callback_;
  base::MockCallback<GetAppAvailabilityCallback> get_app_availability_callback_;
  base::MockCallback<ResultCallback> set_volume_callback_;
  base::MockCallback<ResultCallback> stop_session_callback_;
};

TEST_F(CastMessageHandlerTest, VirtualConnectionCreatedOnlyOnce) {
  ExpectEnsureConnectionThen(CastMessageType::kGetAppAvailability, 2);

  handler_.RequestAppAvailability(
      &cast_socket_, kAppId1,
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
  handler_.RequestAppAvailability(
      &cast_socket_, kAppId2,
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
}

TEST_F(CastMessageHandlerTest, RecreateVirtualConnectionAfterError) {
  ExpectEnsureConnectionThen(CastMessageType::kGetAppAvailability);

  handler_.RequestAppAvailability(
      &cast_socket_, kAppId1,
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  EXPECT_CALL(*this,
              DoOnAppAvailability(kAppId1, GetAppAvailabilityResult::kUnknown));
  OnError(ChannelError::TRANSPORT_ERROR);

  ExpectEnsureConnectionThen(CastMessageType::kGetAppAvailability);

  handler_.RequestAppAvailability(
      &cast_socket_, kAppId2,
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  // The callback is invoked with kUnknown before the PendingRequests is
  // destroyed.
  EXPECT_CALL(*this,
              DoOnAppAvailability(kAppId2, GetAppAvailabilityResult::kUnknown));
}

TEST_F(CastMessageHandlerTest, RequestAppAvailability) {
  ExpectEnsureConnectionThen(CastMessageType::kGetAppAvailability);

  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  std::optional<base::Value::Dict> dict =
      GetDictionaryFromCastMessage(last_request_);
  ASSERT_TRUE(dict);
  const std::optional<int> request_id_value = dict->FindInt("requestId");
  ASSERT_TRUE(request_id_value);
  int request_id = *request_id_value;
  EXPECT_GT(request_id, 0);

  CastMessage response;
  response.set_namespace_("urn:x-cast:com.google.cast.receiver");
  response.set_source_id("receiver-0");
  response.set_destination_id(handler_.source_id());
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
  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(2);
  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
  EXPECT_CALL(*this, DoOnAppAvailability("ABCDEFAB",
                                         GetAppAvailabilityResult::kUnknown));
  task_environment_.FastForwardBy(base::Seconds(5));
}

TEST_F(CastMessageHandlerTest, AppAvailabilitySentOnlyOnceWhilePending) {
  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(2);
  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));

  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(0);
  handler_.RequestAppAvailability(
      &cast_socket_, "ABCDEFAB",
      base::BindOnce(&CastMessageHandlerTest::OnAppAvailability,
                     base::Unretained(this)));
}

TEST_F(CastMessageHandlerTest, EnsureConnection) {
  ExpectEnsureConnection();

  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId,
                            VirtualConnectionType::kStrong);

  // No-op because connection is already created the first time.
  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(0);
  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId,
                            VirtualConnectionType::kStrong);
}

TEST_F(CastMessageHandlerTest, CloseConnection) {
  ExpectEnsureConnection();
  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId,
                            VirtualConnectionType::kStrong);

  EXPECT_CALL(
      *transport_,
      SendMessage_(HasMessageType(CastMessageType::kCloseConnection), _));
  handler_.CloseConnection(channel_id_, kSourceId, kDestinationId);

  // Re-open virtual connection should cause CONNECT message to be sent.
  ExpectEnsureConnection();
  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId,
                            VirtualConnectionType::kStrong);
}

TEST_F(CastMessageHandlerTest, CloseConnectionFromReceiver) {
  ExpectEnsureConnection();
  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId,
                            VirtualConnectionType::kStrong);

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
  EXPECT_CALL(*transport_, SendMessage_(_, _));
  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId,
                            VirtualConnectionType::kStrong);
}

TEST_F(CastMessageHandlerTest, RemoveConnection) {
  ExpectEnsureConnection();
  handler_.EnsureConnection(channel_id_, kSourceId, kDestinationId,
                            VirtualConnectionType::kStrong);

  // Just removing a connection shouldn't send out a close request.
  EXPECT_CALL(
      *transport_,
      SendMessage_(HasMessageType(CastMessageType::kCloseConnection), _))
      .Times(0);
  handler_.RemoveConnection(channel_id_, kSourceId, kDestinationId);
}

TEST_F(CastMessageHandlerTest, LaunchSession) {
  base::HistogramTester histogram_tester;
  cast_socket_.SetFlags(
      static_cast<CastChannelFlags>(CastChannelFlag::kSha1DigestAlgorithm) |
      static_cast<CastChannelFlags>(CastChannelFlag::kCRLMissing));
  ExpectEnsureConnectionThen(CastMessageType::kLaunch);

  const std::optional<base::Value> json = base::JSONReader::Read(kAppParams);

  handler_.LaunchSession(
      channel_id_, kAppId1, base::Seconds(30), {"WEB"}, json,
      base::BindOnce(&CastMessageHandlerTest::ExpectSessionLaunchResult,
                     base::Unretained(this),
                     LaunchSessionResponse::Result::kOk));

  std::optional<base::Value::Dict> dict =
      GetDictionaryFromCastMessage(last_request_);
  ASSERT_TRUE(dict);
  const std::optional<int> request_id_value = dict->FindInt("requestId");
  ASSERT_TRUE(request_id_value);
  int request_id = *request_id_value;
  EXPECT_GT(request_id, 0);
  const base::Value* app_params = dict->Find("appParams");
  EXPECT_EQ(json.value(), *app_params);

  CastMessage response;
  response.set_namespace_("urn:x-cast:com.google.cast.receiver");
  response.set_source_id("receiver-0");
  response.set_destination_id(handler_.source_id());
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
  // Flags associated with the CastSocket should be recorded on launch.
  histogram_tester.ExpectBucketCount(kLaunchSessionChannelFlagsHistogram,
                                     CastChannelFlag::kFlagsNone, 0);
  histogram_tester.ExpectBucketCount(kLaunchSessionChannelFlagsHistogram,
                                     CastChannelFlag::kSha1DigestAlgorithm, 1);
  histogram_tester.ExpectBucketCount(kLaunchSessionChannelFlagsHistogram,
                                     CastChannelFlag::kCRLMissing, 1);
}

TEST_F(CastMessageHandlerTest, LaunchSessionTimedOut) {
  ExpectEnsureConnectionThen(CastMessageType::kLaunch);

  handler_.LaunchSession(
      channel_id_, kAppId1, base::Seconds(30), {"WEB"},
      /* appParams */ std::nullopt,
      base::BindOnce(&CastMessageHandlerTest::ExpectSessionLaunchResult,
                     base::Unretained(this),
                     LaunchSessionResponse::Result::kTimedOut));

  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_EQ(1, session_launch_response_count_);
}

TEST_F(CastMessageHandlerTest, LaunchSessionMessageExceedsSizeLimit) {
  std::string invalid_url(kMaxProtocolMessageSize, 'a');
  base::Value::Dict json;
  json.Set("key", invalid_url);
  handler_.LaunchSession(
      channel_id_, kAppId1, base::Seconds(30), {"WEB"},
      std::make_optional<base::Value>(std::move(json)),
      base::BindOnce(&CastMessageHandlerTest::ExpectSessionLaunchResult,
                     base::Unretained(this),
                     LaunchSessionResponse::Result::kError));
  EXPECT_EQ(1, session_launch_response_count_);
}

TEST_F(CastMessageHandlerTest, SendAppMessage) {
  base::Value::Dict body;
  body.Set("foo", "bar");
  CastMessage message = CreateCastMessage(
      "namespace", base::Value(std::move(body)), kSourceId, kDestinationId);
  {
    InSequence dummy;
    ExpectEnsureConnection();
    EXPECT_CALL(*transport_,
                SendMessage_(HasPayloadUtf8(message.payload_utf8()), _));
  }

  EXPECT_EQ(Result::kOk, handler_.SendAppMessage(channel_id_, message));
}

TEST_F(CastMessageHandlerTest, SendMessageOnInvisibleConnection) {
  // For destinations other than receiver-0, we should default to an invisible
  // connection.
  SendMessageAndExpectConnection("non-platform-receiver-id",
                                 VirtualConnectionType::kInvisible);
}

TEST_F(CastMessageHandlerTest, SendMessageToPlatformReceiver) {
  // For receiver-0, we should default to a strong connection because some
  // commands (e.g. LAUNCH) are not accepted from invisible connections.
  SendMessageAndExpectConnection("receiver-0", VirtualConnectionType::kStrong);
}

TEST_F(CastMessageHandlerTest, SendAppMessageExceedsSizeLimit) {
  std::string invalid_msg(kMaxProtocolMessageSize, 'a');
  base::Value::Dict body;
  body.Set("foo", invalid_msg);
  CastMessage message = CreateCastMessage(
      "namespace", base::Value(std::move(body)), kSourceId, kDestinationId);

  EXPECT_EQ(Result::kFailed, handler_.SendAppMessage(channel_id_, message));
}

// Check that SendMediaRequest sends a message created by CreateMediaRequest and
// returns a request ID.
TEST_F(CastMessageHandlerTest, SendMediaRequest) {
  {
    InSequence dummy;
    ExpectEnsureConnection();
    EXPECT_CALL(*transport_, SendMessage_(_, _))
        .WillOnce(WithArg<0>([&](const auto& message) {
          std::string expected_body = R"({
            "requestId": 1,
            "type": "PLAY",
          })";
          auto expected = CreateMediaRequest(ParseJsonDict(expected_body), 1,
                                             "theSourceId", kDestinationId);
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
  std::optional<int> request_id = handler_.SendMediaRequest(
      channel_id_, ParseJsonDict(message_str), "theSourceId", kDestinationId);
  EXPECT_EQ(1, request_id);
}

// Check that SendVolumeCommand sends a message created by CreateVolumeRequest
// and registers a pending request.
TEST_F(CastMessageHandlerTest, SendVolumeCommand) {
  {
    InSequence dummy;
    ExpectEnsureConnection();
    EXPECT_CALL(*transport_, SendMessage_(_, _))
        .WillOnce(WithArg<0>([&](const auto& message) {
          std::string expected_body = R"({
            "requestId": 1,
            "type": "SET_VOLUME",
          })";
          auto expected = CreateSetVolumeRequest(ParseJsonDict(expected_body),
                                                 1, "theSourceId");
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
  handler_.SendSetVolumeRequest(channel_id_, ParseJsonDict(message_str),
                                "theSourceId", base::DoNothing());
}

// Check that closing a socket removes pending requests, and that the pending
// request callbacks are called appropriately.
TEST_F(CastMessageHandlerTest, PendingRequestsDestructor) {
  CreatePendingRequests();

  // Set up expanctions for pending request callbacks.
  EXPECT_CALL(launch_session_callback_, Run(_, _))
      .WillOnce(WithArg<0>([&](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::kError, response.result);
        EXPECT_EQ(std::nullopt, response.receiver_status);
      }));
  EXPECT_CALL(get_app_availability_callback_,
              Run(kAppId1, GetAppAvailabilityResult::kUnknown))
      .Times(2);
  EXPECT_CALL(set_volume_callback_, Run(Result::kFailed)).Times(2);
  EXPECT_CALL(stop_session_callback_, Run(Result::kFailed));

  // Force callbacks to be called through PendingRequests destructor by
  // simulating a socket closing.
  EXPECT_CALL(cast_socket_, ready_state()).WillOnce(Return(ReadyState::CLOSED));
  handler_.OnReadyStateChanged(cast_socket_);
}

TEST_F(CastMessageHandlerTest, HandlePendingRequest) {
  int next_request_id = 1;
  CreatePendingRequests();

  // Set up expanctions for pending request callbacks.
  EXPECT_CALL(launch_session_callback_, Run(_, _))
      .WillOnce(WithArg<0>([&](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::kOk, response.result);
        EXPECT_THAT(response.receiver_status,
                    testing::Optional(IsJson(R"({"foo": "bar"})")));
      }));
  EXPECT_CALL(get_app_availability_callback_,
              Run(kAppId1, GetAppAvailabilityResult::kAvailable))
      .Times(2);
  EXPECT_CALL(set_volume_callback_, Run(Result::kOk)).Times(2);
  EXPECT_CALL(stop_session_callback_, Run(Result::kOk));

  HandlePendingLaunchSessionRequest(next_request_id++);
  // Handle both pending get app availability requests.
  HandleAppAvailabilityRequest(next_request_id++);
  // Handle pending set volume request (1 of 2).
  HandlePendingGeneralRequest(next_request_id++);
  // Skip request_id == 4, since it was used by the second get app availability
  // request.
  next_request_id++;
  // Handle pending set volume request (2 of 2).
  HandlePendingGeneralRequest(next_request_id++);
  // Handle pending stop session request.
  HandlePendingGeneralRequest(next_request_id++);
}

// Check that set volume requests time out correctly.
TEST_F(CastMessageHandlerTest, SetVolumeTimedOut) {
  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(AnyNumber());

  std::string message_str = R"({
    "sessionId": "theSessionId",
    "type": "SET_VOLUME",
  })";
  base::MockCallback<ResultCallback> callback;
  handler_.SendSetVolumeRequest(channel_id_, ParseJsonDict(message_str),
                                "theSourceId", callback.Get());
  EXPECT_CALL(callback, Run(Result::kFailed));
  task_environment_.FastForwardBy(kRequestTimeout);
}

TEST_F(CastMessageHandlerTest, SendMultipleLaunchRequests) {
  int next_request_id = 1;
  base::MockCallback<LaunchSessionCallback> expect_success_callback;
  base::MockCallback<LaunchSessionCallback> expect_failure_callback;

  EXPECT_CALL(expect_success_callback, Run(_, _))
      .WillOnce(WithArg<0>([](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::Result::kOk, response.result);
      }));
  EXPECT_CALL(expect_failure_callback, Run(_, _))
      .WillOnce(WithArg<0>([](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::Result::kError, response.result);
      }));
  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(AnyNumber());
  handler_.LaunchSession(channel_id_, kAppId1, base::TimeDelta::Max(), {"WEB"},
                         /* appParams */ std::nullopt,
                         expect_success_callback.Get());
  // When there already is a launch request queued, we expect subsequent
  // requests to fail.
  handler_.LaunchSession(channel_id_, kAppId1, base::TimeDelta::Max(), {"WEB"},
                         /* appParams */ std::nullopt,
                         expect_failure_callback.Get());
  // This resolves the first launch request.
  HandlePendingLaunchSessionRequest(next_request_id++);
}

TEST_F(CastMessageHandlerTest, SendMultipleStopRequests) {
  int next_request_id = 1;
  base::MockCallback<ResultCallback> expect_success_callback;
  base::MockCallback<ResultCallback> expect_failure_callback;

  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(AnyNumber());
  handler_.LaunchSession(channel_id_, kAppId1, base::TimeDelta::Max(), {"WEB"},
                         /* appParams */ std::nullopt,
                         launch_session_callback_.Get());
  EXPECT_CALL(launch_session_callback_, Run(_, _))
      .WillOnce(WithArg<0>([&](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::kOk, response.result);
      }));
  HandlePendingLaunchSessionRequest(next_request_id++);

  EXPECT_CALL(expect_success_callback, Run(Result::kOk));
  EXPECT_CALL(expect_failure_callback, Run(Result::kFailed));
  handler_.StopSession(channel_id_, kSessionId, kSourceId,
                       expect_success_callback.Get());
  // When there already is a stop request queued, we expect subsequent requests
  // to fail.
  handler_.StopSession(channel_id_, kSessionId, kSourceId,
                       expect_failure_callback.Get());
  // This resolves the first stop request.
  HandlePendingGeneralRequest(next_request_id++);
}

TEST_F(CastMessageHandlerTest, LaunchSessionWithPromptUserAllowed) {
  int request_id = 1;

  base::MockCallback<LaunchSessionCallback> expect_user_prompt_callback;
  base::MockCallback<LaunchSessionCallback> expect_user_allowed_callback;

  EXPECT_CALL(expect_user_prompt_callback, Run(_, _))
      .WillOnce([&](LaunchSessionResponse response,
                    LaunchSessionCallbackWrapper* out_callback) {
        EXPECT_EQ(LaunchSessionResponse::Result::kPendingUserAuth,
                  response.result);
        out_callback->callback = expect_user_allowed_callback.Get();
      });
  EXPECT_CALL(expect_user_allowed_callback, Run(_, _))
      .WillOnce([&](LaunchSessionResponse response,
                    LaunchSessionCallbackWrapper* out_callback) {
        EXPECT_EQ(LaunchSessionResponse::Result::kUserAllowed, response.result);
        out_callback->callback = launch_session_callback_.Get();
      });
  EXPECT_CALL(launch_session_callback_, Run(_, _))
      .WillOnce(WithArg<0>([](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::Result::kOk, response.result);
      }));

  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(AnyNumber());
  handler_.LaunchSession(channel_id_, kAppId1, base::TimeDelta::Max(), {"WEB"},
                         /* appParams */ std::nullopt,
                         expect_user_prompt_callback.Get());

  HandleLaunchStatusResponse(request_id, kWaitingUserResponse);
  HandleLaunchStatusResponse(request_id, kUserAllowedStatus);
  HandlePendingLaunchSessionRequest(request_id);
}

TEST_F(CastMessageHandlerTest, LaunchSessionWithPromptUserNotAllowed) {
  int request_id = 1;

  base::MockCallback<LaunchSessionCallback> expect_user_prompt_callback;

  EXPECT_CALL(expect_user_prompt_callback, Run(_, _))
      .WillOnce([&](LaunchSessionResponse response,
                    LaunchSessionCallbackWrapper* out_callback) {
        EXPECT_EQ(LaunchSessionResponse::Result::kPendingUserAuth,
                  response.result);
        out_callback->callback = launch_session_callback_.Get();
      });
  EXPECT_CALL(launch_session_callback_, Run(_, _))
      .WillOnce(WithArg<0>([](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::Result::kUserNotAllowed,
                  response.result);
      }));

  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(AnyNumber());
  handler_.LaunchSession(channel_id_, kAppId1, base::TimeDelta::Max(), {"WEB"},
                         /* appParams */ std::nullopt,
                         expect_user_prompt_callback.Get());

  HandleLaunchStatusResponse(request_id, kWaitingUserResponse);
  HandleLaunchErrorResponse(request_id, kUserNotAllowedError);
}

TEST_F(CastMessageHandlerTest,
       LaunchSessionWithPromptUserNotificationDisabled) {
  int request_id = 1;

  EXPECT_CALL(launch_session_callback_, Run(_, _))
      .WillOnce(WithArg<0>([](LaunchSessionResponse response) {
        EXPECT_EQ(LaunchSessionResponse::Result::kNotificationDisabled,
                  response.result);
      }));

  EXPECT_CALL(*transport_, SendMessage_(_, _)).Times(AnyNumber());
  handler_.LaunchSession(channel_id_, kAppId1, base::TimeDelta::Max(), {"WEB"},
                         /* appParams */ std::nullopt,
                         launch_session_callback_.Get());

  HandleLaunchErrorResponse(request_id, kNotificationDisabledError);
}

}  // namespace cast_channel

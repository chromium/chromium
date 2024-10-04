// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_sender.h"

#include <optional>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/proto/babel_orca_message.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon_enums.pb.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_constants.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

constexpr char kLanguage[] = "en-US";
constexpr int64_t kInitTimestampMs = 1724792276909;

struct TranscriptSenderTestCase {
  std::string test_name;
  std::string transcript1;
  std::string transcript2;
  size_t max_allowed_char;
  std::string expected_sent_transcript;
  int expected_index;
};

using TranscriptSenderTest = testing::TestWithParam<TranscriptSenderTestCase>;

void VerifyTranscriptPartProto(const TranscriptPart& transcript_part,
                               int transcript_id,
                               const std::string& transcript_text,
                               int index,
                               bool is_final,
                               const std::string& language) {
  EXPECT_EQ(transcript_part.transcript_id(), transcript_id);
  EXPECT_EQ(transcript_part.text(), transcript_text);
  EXPECT_EQ(transcript_part.text_index(), index);
  EXPECT_EQ(transcript_part.is_final(), is_final);
  EXPECT_EQ(transcript_part.language(), language);
}

TEST(TranscriptSenderTest, SendOneMessageLongerThanMaxAllowed) {
  base::test::TaskEnvironment task_environment;
  const int kMaxAllowedChar = 5;
  const std::string kTranscriptText = "hello transcription";
  FakeTachyonAuthedClient authed_client;
  FakeTachyonRequestDataProvider request_data_provider;
  base::test::TestFuture<void> failure_future;
  TranscriptSender sender(
      &authed_client, &request_data_provider,
      base::Time::FromMillisecondsSinceUnixEpoch(kInitTimestampMs),
      TRAFFIC_ANNOTATION_FOR_TESTS, {.max_allowed_char = kMaxAllowedChar},
      failure_future.GetCallback());

  media::SpeechRecognitionResult transcript(kTranscriptText,
                                            /*is_final=*/false);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript, kLanguage));
  authed_client.WaitForRequest();
  authed_client.ExecuteResponseCallback(TachyonResponse(
      net::OK, net::HttpStatusCode::HTTP_OK,
      std::make_unique<std::string>(InboxSendResponse().SerializeAsString())));

  EXPECT_FALSE(failure_future.IsReady());
  InboxSendRequest sent_request;
  ASSERT_TRUE(sent_request.ParseFromString(authed_client.GetRequestString()));
  EXPECT_EQ(sent_request.header().auth_token_payload(),
            request_data_provider.tachyon_token());

  // dest_id
  EXPECT_EQ(sent_request.dest_id().id(), request_data_provider.group_id());
  EXPECT_EQ(sent_request.dest_id().type(), IdType::GROUP_ID);
  EXPECT_THAT(sent_request.dest_id().app(), testing::StrEq(kTachyonAppName));

  EXPECT_EQ(sent_request.fanout_sender(), MessageFanout::OTHER_SENDER_DEVICES);

  // sender_id
  EXPECT_EQ(sent_request.message().sender_id().id(),
            request_data_provider.sender_email());
  EXPECT_EQ(sent_request.message().sender_id().type(), IdType::EMAIL);
  EXPECT_THAT(sent_request.message().sender_id().app(),
              testing::StrEq(kTachyonAppName));

  // receiver_id
  EXPECT_EQ(sent_request.message().receiver_id().id(),
            request_data_provider.group_id());
  EXPECT_EQ(sent_request.message().receiver_id().type(), IdType::GROUP_ID);
  EXPECT_THAT(sent_request.message().receiver_id().app(),
              testing::StrEq(kTachyonAppName));

  EXPECT_EQ(sent_request.message().message_type(), InboxMessage::GROUP);
  EXPECT_EQ(sent_request.message().message_class(), InboxMessage::EPHEMERAL);

  BabelOrcaMessage message;
  ASSERT_TRUE(message.ParseFromString(sent_request.message().message()));
  EXPECT_EQ(message.session_id(), request_data_provider.session_id());
  EXPECT_EQ(message.order(), 0);
  EXPECT_FALSE(message.has_previous_transcript());
  ASSERT_TRUE(message.has_current_transcript());
  VerifyTranscriptPartProto(message.current_transcript(), /*transcript_id=*/0,
                            kTranscriptText, /*index=*/0, /*is_final=*/false,
                            kLanguage);
}

TEST(TranscriptSenderTest, SendNewTranscript) {
  base::test::TaskEnvironment task_environment;
  const int kMaxAllowedChar = 26;
  const std::string kTranscriptText = "hello1 hello2 hello3";
  FakeTachyonAuthedClient authed_client;
  std::string request_string1;
  std::string request_string2;
  FakeTachyonRequestDataProvider request_data_provider;
  base::test::TestFuture<void> failure_future;
  TranscriptSender sender(
      &authed_client, &request_data_provider,
      base::Time::FromMillisecondsSinceUnixEpoch(kInitTimestampMs),
      TRAFFIC_ANNOTATION_FOR_TESTS, {.max_allowed_char = kMaxAllowedChar},
      failure_future.GetCallback());

  media::SpeechRecognitionResult transcript1(kTranscriptText,
                                             /*is_final=*/true);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript1, kLanguage));
  authed_client.WaitForRequest();
  request_string1 = authed_client.GetRequestString();
  authed_client.ExecuteResponseCallback(TachyonResponse(
      net::OK, net::HttpStatusCode::HTTP_OK,
      std::make_unique<std::string>(InboxSendResponse().SerializeAsString())));

  media::SpeechRecognitionResult transcript2(kTranscriptText,
                                             /*is_final=*/false);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript2, kLanguage));
  authed_client.WaitForRequest();
  request_string2 = authed_client.GetRequestString();
  authed_client.ExecuteResponseCallback(TachyonResponse(
      net::OK, net::HttpStatusCode::HTTP_OK,
      std::make_unique<std::string>(InboxSendResponse().SerializeAsString())));

  EXPECT_FALSE(failure_future.IsReady());
  InboxSendRequest sent_request1;
  BabelOrcaMessage message1;
  ASSERT_TRUE(sent_request1.ParseFromString(request_string1));
  ASSERT_TRUE(message1.ParseFromString(sent_request1.message().message()));
  EXPECT_EQ(message1.order(), 0);
  EXPECT_FALSE(message1.has_previous_transcript());
  ASSERT_TRUE(message1.has_current_transcript());
  VerifyTranscriptPartProto(message1.current_transcript(), /*transcript_id=*/0,
                            kTranscriptText, /*index=*/0, /*is_final=*/true,
                            kLanguage);

  InboxSendRequest sent_request2;
  BabelOrcaMessage message2;
  ASSERT_TRUE(sent_request2.ParseFromString(request_string2));
  ASSERT_TRUE(message2.ParseFromString(sent_request2.message().message()));
  EXPECT_EQ(message2.order(), 1);
  ASSERT_TRUE(message2.has_current_transcript());
  VerifyTranscriptPartProto(message2.current_transcript(), /*transcript_id=*/1,
                            kTranscriptText, /*index=*/0, /*is_final=*/false,
                            kLanguage);
  ASSERT_TRUE(message2.has_previous_transcript());
  VerifyTranscriptPartProto(message2.previous_transcript(), /*transcript_id=*/0,
                            "hello3", /*index=*/14, /*is_final=*/true,
                            kLanguage);
  EXPECT_EQ(message1.init_timestamp_ms(), kInitTimestampMs);
  EXPECT_EQ(message2.init_timestamp_ms(), kInitTimestampMs);
}

TEST(TranscriptSenderTest, RejectSendingAndReplyOnMaxErrorsReached) {
  base::test::TaskEnvironment task_environment;
  const int kMaxAllowedChar = 26;
  const std::string kTranscriptText = "hello1 hello2 hello3";
  FakeTachyonAuthedClient authed_client;
  FakeTachyonRequestDataProvider request_data_provider;
  base::test::TestFuture<void> failure_future;
  TranscriptSender sender(
      &authed_client, &request_data_provider,
      base::Time::FromMillisecondsSinceUnixEpoch(kInitTimestampMs),
      TRAFFIC_ANNOTATION_FOR_TESTS,
      {.max_allowed_char = kMaxAllowedChar, .max_errors_num = 2},
      failure_future.GetCallback());

  media::SpeechRecognitionResult transcript1(kTranscriptText,
                                             /*is_final=*/true);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript1, kLanguage));
  authed_client.WaitForRequest();
  authed_client.ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kHttpError));

  media::SpeechRecognitionResult transcript2(kTranscriptText,
                                             /*is_final=*/false);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript2, kLanguage));
  authed_client.WaitForRequest();
  authed_client.ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kHttpError));

  media::SpeechRecognitionResult transcript3(kTranscriptText,
                                             /*is_final=*/false);
  EXPECT_FALSE(sender.SendTranscriptionUpdate(transcript3, kLanguage));

  EXPECT_TRUE(failure_future.IsReady());
}

TEST(TranscriptSenderTest, ResetErrorCountOnSuccess) {
  base::test::TaskEnvironment task_environment;
  const int kMaxAllowedChar = 26;
  const std::string kTranscriptText = "hello1 hello2 hello3";
  FakeTachyonAuthedClient authed_client;
  FakeTachyonRequestDataProvider request_data_provider;
  base::test::TestFuture<void> failure_future;
  TranscriptSender sender(
      &authed_client, &request_data_provider,
      base::Time::FromMillisecondsSinceUnixEpoch(kInitTimestampMs),
      TRAFFIC_ANNOTATION_FOR_TESTS,
      {.max_allowed_char = kMaxAllowedChar, .max_errors_num = 2},
      failure_future.GetCallback());

  // Failed request
  media::SpeechRecognitionResult transcript1(kTranscriptText,
                                             /*is_final=*/true);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript1, kLanguage));
  authed_client.WaitForRequest();
  authed_client.ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kHttpError));

  // Successful request, should reset error count.
  media::SpeechRecognitionResult transcript2(kTranscriptText,
                                             /*is_final=*/false);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript2, kLanguage));
  authed_client.WaitForRequest();
  authed_client.ExecuteResponseCallback(TachyonResponse(
      net::OK, net::HttpStatusCode::HTTP_OK,
      std::make_unique<std::string>(InboxSendResponse().SerializeAsString())));

  // Failed request, should not trigger failure callback since the error count
  // was reset.
  media::SpeechRecognitionResult transcript3(kTranscriptText,
                                             /*is_final=*/false);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript3, kLanguage));
  authed_client.WaitForRequest();
  authed_client.ExecuteResponseCallback(
      TachyonResponse(TachyonResponse::Status::kHttpError));

  EXPECT_FALSE(failure_future.IsReady());
}

TEST(TranscriptSenderTest, InflightRequestsAreHandledOnFailure) {
  base::test::TaskEnvironment task_environment;
  const int kMaxAllowedChar = 26;
  const std::string kTranscriptText = "hello1 hello2 hello3";
  FakeTachyonAuthedClient authed_client;
  FakeTachyonRequestDataProvider request_data_provider;
  base::test::TestFuture<void> failure_future;
  TranscriptSender sender(
      &authed_client, &request_data_provider,
      base::Time::FromMillisecondsSinceUnixEpoch(kInitTimestampMs),
      TRAFFIC_ANNOTATION_FOR_TESTS,
      {.max_allowed_char = kMaxAllowedChar, .max_errors_num = 2},
      failure_future.GetCallback());

  media::SpeechRecognitionResult transcript1(kTranscriptText,
                                             /*is_final=*/true);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript1, kLanguage));
  authed_client.WaitForRequest();
  RequestDataWrapper::ResponseCallback response_cb1 =
      authed_client.TakeResponseCallback();

  media::SpeechRecognitionResult transcript2(kTranscriptText,
                                             /*is_final=*/false);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript2, kLanguage));
  authed_client.WaitForRequest();
  RequestDataWrapper::ResponseCallback response_cb2 =
      authed_client.TakeResponseCallback();

  media::SpeechRecognitionResult transcript3(kTranscriptText,
                                             /*is_final=*/false);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript3, kLanguage));
  authed_client.WaitForRequest();
  RequestDataWrapper::ResponseCallback response_cb3 =
      authed_client.TakeResponseCallback();

  std::move(response_cb1)
      .Run(TachyonResponse(TachyonResponse::Status::kHttpError));
  std::move(response_cb2)
      .Run(TachyonResponse(TachyonResponse::Status::kHttpError));

  EXPECT_TRUE(failure_future.IsReady());

  std::move(response_cb3)
      .Run(TachyonResponse(TachyonResponse::Status::kHttpError));

  media::SpeechRecognitionResult transcript4(kTranscriptText,
                                             /*is_final=*/false);
  EXPECT_FALSE(sender.SendTranscriptionUpdate(transcript3, kLanguage));
}

TEST_P(TranscriptSenderTest, SendTwoMessages) {
  base::test::TaskEnvironment task_environment;
  FakeTachyonAuthedClient authed_client;
  InboxSendRequest sent_request2;
  FakeTachyonRequestDataProvider request_data_provider;
  TranscriptSender sender(
      &authed_client, &request_data_provider,
      base::Time::FromMillisecondsSinceUnixEpoch(kInitTimestampMs),
      TRAFFIC_ANNOTATION_FOR_TESTS,
      {.max_allowed_char = GetParam().max_allowed_char}, base::DoNothing());

  media::SpeechRecognitionResult transcript1(GetParam().transcript1,
                                             /*is_final=*/false);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript1, kLanguage));
  authed_client.WaitForRequest();
  media::SpeechRecognitionResult transcript2(GetParam().transcript2,
                                             /*is_final=*/true);
  EXPECT_TRUE(sender.SendTranscriptionUpdate(transcript2, kLanguage));
  authed_client.WaitForRequest();

  BabelOrcaMessage message2;
  ASSERT_TRUE(sent_request2.ParseFromString(authed_client.GetRequestString()));
  ASSERT_TRUE(message2.ParseFromString(sent_request2.message().message()));
  VerifyTranscriptPartProto(message2.current_transcript(), /*transcript_id=*/0,
                            GetParam().expected_sent_transcript,
                            /*index=*/GetParam().expected_index,
                            /*is_final=*/true, kLanguage);
  EXPECT_FALSE(message2.has_previous_transcript());
}

INSTANTIATE_TEST_SUITE_P(
    TranscriptSenderTestSuiteInstantiation,
    TranscriptSenderTest,
    testing::ValuesIn<TranscriptSenderTestCase>({
        {"DiffShorterThanMaxAllowed", "hello", "hello world", 7, "o world", 4},
        {"DiffLongerThanMaxAllowed", "hello", "hello world", 1, " world", 5},
    }),
    [](const testing::TestParamInfo<TranscriptSenderTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash::babelorca

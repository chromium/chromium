// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/transcript_sender.h"

#include <string>

#include "base/test/task_environment.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_authed_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_client.h"
#include "chromeos/ash/components/boca/babelorca/fakes/fake_tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/proto/babel_orca_message.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon_enums.pb.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_constants.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

constexpr char kSenderEmail[] = "sender@test.com";
constexpr char kLanguage[] = "en-US";

struct TranscriptSenderTestCase {
  std::string test_name;
  std::string transcript1;
  std::string transcript2;
  int max_allowed_char;
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
  TranscriptSender sender(&authed_client, &request_data_provider, kSenderEmail,
                          TRAFFIC_ANNOTATION_FOR_TESTS, kMaxAllowedChar);

  media::SpeechRecognitionResult transcript(kTranscriptText,
                                            /*is_final=*/false);
  sender.SendTranscriptionUpdate(transcript, kLanguage);
  authed_client.WaitForRequest();

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
  EXPECT_EQ(sent_request.message().sender_id().id(), kSenderEmail);
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
  EXPECT_EQ(sent_request.message().message_class(), InboxMessage::USER);

  BabelOrcaMessage message;
  ASSERT_TRUE(message.ParseFromString(sent_request.message().message()));
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
  TranscriptSender sender(&authed_client, &request_data_provider, kSenderEmail,
                          TRAFFIC_ANNOTATION_FOR_TESTS, kMaxAllowedChar);

  media::SpeechRecognitionResult transcript1(kTranscriptText,
                                             /*is_final=*/true);
  sender.SendTranscriptionUpdate(transcript1, kLanguage);
  authed_client.WaitForRequest();
  request_string1 = authed_client.GetRequestString();
  media::SpeechRecognitionResult transcript2(kTranscriptText,
                                             /*is_final=*/false);
  sender.SendTranscriptionUpdate(transcript2, kLanguage);
  authed_client.WaitForRequest();
  request_string2 = authed_client.GetRequestString();

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
  EXPECT_EQ(message1.sender_uuid(), message2.sender_uuid());
}

TEST_P(TranscriptSenderTest, SendTwoMessages) {
  base::test::TaskEnvironment task_environment;
  FakeTachyonAuthedClient authed_client;
  InboxSendRequest sent_request2;
  FakeTachyonRequestDataProvider request_data_provider;
  TranscriptSender sender(&authed_client, &request_data_provider, kSenderEmail,
                          TRAFFIC_ANNOTATION_FOR_TESTS,
                          GetParam().max_allowed_char);

  media::SpeechRecognitionResult transcript1(GetParam().transcript1,
                                             /*is_final=*/false);
  sender.SendTranscriptionUpdate(transcript1, kLanguage);
  authed_client.WaitForRequest();
  media::SpeechRecognitionResult transcript2(GetParam().transcript2,
                                             /*is_final=*/true);
  sender.SendTranscriptionUpdate(transcript2, kLanguage);
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

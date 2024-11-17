// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/boca/babelorca/cpp/tachyon_parsing_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/boca/babelorca/proto/babel_orca_message.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/stream_body.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/tachyon_enums.pb.h"
#include "chromeos/ash/components/boca/babelorca/proto/testing_message.pb.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom-shared.h"
#include "chromeos/ash/services/boca/babelorca/mojom/tachyon_parsing_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

constexpr char kSenderEmail[] = "sender@email.com";

class TachyonParsingServiceTest : public testing::Test {
 protected:
  TranscriptPart CreateTranscriptPart(int id,
                                      int index,
                                      std::string text,
                                      bool is_final,
                                      std::string lang) {
    TranscriptPart transcript_part;
    transcript_part.set_transcript_id(id);
    transcript_part.set_text_index(index);
    transcript_part.set_text(std::move(text));
    transcript_part.set_is_final(is_final);
    transcript_part.set_language(lang);
    return transcript_part;
  }

  BabelOrcaMessage CreateBabelOrcaMessage(const std::string& session_id,
                                          int init_timestamp_ms,
                                          int order,
                                          bool has_previous_transcript = true) {
    BabelOrcaMessage babel_orca_message;
    babel_orca_message.set_session_id(session_id);
    babel_orca_message.set_init_timestamp_ms(init_timestamp_ms);
    babel_orca_message.set_order(order);
    *babel_orca_message.mutable_current_transcript() =
        CreateTranscriptPart(/*id=*/6, /*index=*/10, /*text=*/"text",
                             /*is_final=*/false, /*lang=*/"en");
    if (has_previous_transcript) {
      *babel_orca_message.mutable_previous_transcript() =
          CreateTranscriptPart(/*id=*/3, /*index=*/8, /*text=*/"previous text",
                               /*is_final=*/true, /*lang=*/"en");
    }
    return babel_orca_message;
  }

  ReceiveMessagesResponse CreateReceivedMessage(
      const BabelOrcaMessage& babel_orca_message_proto) {
    ReceiveMessagesResponse received_message;
    InboxMessage* inbox_message = received_message.mutable_inbox_message();
    inbox_message->mutable_sender_id()->set_type(IdType::EMAIL);
    inbox_message->mutable_sender_id()->set_id(kSenderEmail);
    inbox_message->set_message(babel_orca_message_proto.SerializeAsString());
    return received_message;
  }

  void VerifyTranscriptPart(const mojom::TranscriptPartPtr& mojom_transcript,
                            const TranscriptPart& proto_transcript) {
    EXPECT_EQ(mojom_transcript->transcript_id,
              proto_transcript.transcript_id());
    EXPECT_EQ(mojom_transcript->text_index, proto_transcript.text_index());
    EXPECT_EQ(mojom_transcript->text, proto_transcript.text());
    EXPECT_EQ(mojom_transcript->is_final, proto_transcript.is_final());
    EXPECT_EQ(mojom_transcript->language, proto_transcript.language());
  }

  void VerifyBabelOrcaMessage(const mojom::BabelOrcaMessagePtr& mojom_message,
                              const BabelOrcaMessage& proto_message) {
    EXPECT_EQ(mojom_message->session_id, proto_message.session_id());
    EXPECT_EQ(mojom_message->init_timestamp_ms,
              proto_message.init_timestamp_ms());
    EXPECT_EQ(mojom_message->order, proto_message.order());
  }

  base::test::TaskEnvironment task_environment_;
  base::test::TestFuture<mojom::ParsingState,
                         std::vector<mojom::BabelOrcaMessagePtr>,
                         mojom::StreamStatusPtr>
      parse_future_;
  mojo::Remote<mojom::TachyonParsingService> parsing_remote_;
};

TEST_F(TachyonParsingServiceTest, OneMessageOverOneRequest) {
  TachyonParsingService service(parsing_remote_.BindNewPipeAndPassReceiver());
  BabelOrcaMessage babel_orca_message_proto = CreateBabelOrcaMessage(
      /*session_id=*/"session_id", /*init_timestamp_ms=*/123456, /*order=*/5);
  ReceiveMessagesResponse received_message =
      CreateReceivedMessage(babel_orca_message_proto);
  StreamBody stream_body;
  stream_body.add_messages(received_message.SerializeAsString());

  parsing_remote_->Parse(stream_body.SerializeAsString(),
                         parse_future_.GetCallback());
  auto [state, messages, stream_status] = parse_future_.Take();

  EXPECT_EQ(state, mojom::ParsingState::kOk);
  EXPECT_TRUE(stream_status.is_null());
  ASSERT_THAT(messages, testing::SizeIs(1));
  VerifyBabelOrcaMessage(messages[0], babel_orca_message_proto);
  VerifyTranscriptPart(messages[0]->current_transcript,
                       babel_orca_message_proto.current_transcript());
  ASSERT_FALSE(messages[0]->previous_transcript.is_null());
  VerifyTranscriptPart(messages[0]->previous_transcript,
                       babel_orca_message_proto.previous_transcript());
  ASSERT_TRUE(messages[0]->sender_email.has_value());
  EXPECT_THAT(messages[0]->sender_email.value(), testing::StrEq(kSenderEmail));
}

TEST_F(TachyonParsingServiceTest, PartialMessage) {
  TachyonParsingService service(parsing_remote_.BindNewPipeAndPassReceiver());
  BabelOrcaMessage babel_orca_message_proto = CreateBabelOrcaMessage(
      /*session_id=*/"session_id", /*init_timestamp_ms=*/123456, /*order=*/5);
  ReceiveMessagesResponse received_message =
      CreateReceivedMessage(babel_orca_message_proto);
  StreamBody stream_body;
  stream_body.add_messages(received_message.SerializeAsString());
  std::string stream_string = stream_body.SerializeAsString();

  parsing_remote_->Parse(stream_string.substr(0, stream_string.length() / 2),
                         parse_future_.GetCallback());
  auto [state, messages, stream_status] = parse_future_.Take();

  EXPECT_EQ(state, mojom::ParsingState::kOk);
  EXPECT_THAT(messages, testing::IsEmpty());
  EXPECT_TRUE(stream_status.is_null());
}

TEST_F(TachyonParsingServiceTest, MultipleMessagesOverOneRequest) {
  TachyonParsingService service(parsing_remote_.BindNewPipeAndPassReceiver());
  BabelOrcaMessage babel_orca_message_proto1 = CreateBabelOrcaMessage(
      /*session_id=*/"session_id1", /*init_timestamp_ms=*/123456, /*order=*/5);
  ReceiveMessagesResponse received_message1 =
      CreateReceivedMessage(babel_orca_message_proto1);
  BabelOrcaMessage babel_orca_message_proto2 = CreateBabelOrcaMessage(
      /*session_id=*/"session_id2", /*init_timestamp_ms=*/909, /*order=*/19,
      /*has_previous_transcript=*/false);
  ReceiveMessagesResponse received_message2 =
      CreateReceivedMessage(babel_orca_message_proto2);
  StreamBody stream_message_body;
  stream_message_body.add_messages(received_message1.SerializeAsString());
  stream_message_body.add_messages(received_message2.SerializeAsString());

  parsing_remote_->Parse(stream_message_body.SerializeAsString(),
                         parse_future_.GetCallback());
  auto [state, messages, stream_status] = parse_future_.Take();

  EXPECT_EQ(state, mojom::ParsingState::kOk);
  EXPECT_TRUE(stream_status.is_null());
  ASSERT_THAT(messages, testing::SizeIs(2));
  // First message
  VerifyBabelOrcaMessage(messages[0], babel_orca_message_proto1);
  VerifyTranscriptPart(messages[0]->current_transcript,
                       babel_orca_message_proto1.current_transcript());
  ASSERT_FALSE(messages[0]->previous_transcript.is_null());
  VerifyTranscriptPart(messages[0]->previous_transcript,
                       babel_orca_message_proto1.previous_transcript());
  // Second message
  VerifyBabelOrcaMessage(messages[1], babel_orca_message_proto2);
  VerifyTranscriptPart(messages[1]->current_transcript,
                       babel_orca_message_proto2.current_transcript());
  EXPECT_TRUE(messages[1]->previous_transcript.is_null());
}

TEST_F(TachyonParsingServiceTest, MultipleMessagesAndStatusOverOneRequest) {
  TachyonParsingService service(parsing_remote_.BindNewPipeAndPassReceiver());
  BabelOrcaMessage babel_orca_message_proto1 = CreateBabelOrcaMessage(
      /*session_id=*/"session_id1", /*init_timestamp_ms=*/123456,
      /*order=*/5);
  ReceiveMessagesResponse received_message1 =
      CreateReceivedMessage(babel_orca_message_proto1);
  BabelOrcaMessage babel_orca_message_proto2 = CreateBabelOrcaMessage(
      /*session_id=*/"session_id2", /*init_timestamp_ms=*/909, /*order=*/19,
      /*has_previous_transcript=*/false);
  ReceiveMessagesResponse received_message2 =
      CreateReceivedMessage(babel_orca_message_proto2);
  StreamBody stream_status;
  stream_status.mutable_status()->set_code(200);
  stream_status.mutable_status()->set_message("close");
  StreamBody stream_message_body;
  stream_message_body.add_messages(received_message1.SerializeAsString());
  stream_message_body.add_messages(received_message2.SerializeAsString());
  std::string stream_message_string = stream_message_body.SerializeAsString();

  parsing_remote_->Parse(
      base::StrCat({stream_message_string, stream_status.SerializeAsString()}),
      parse_future_.GetCallback());
  auto [state1, messages1, stream_status1] = parse_future_.Take();
  // New calls should not parse and should keep returning `kClosed`.
  parsing_remote_->Parse(stream_message_string, parse_future_.GetCallback());
  auto [state2, messages2, stream_status2] = parse_future_.Take();

  EXPECT_EQ(state1, mojom::ParsingState::kClosed);
  ASSERT_THAT(messages1, testing::SizeIs(2));
  ASSERT_FALSE(stream_status1.is_null());
  EXPECT_EQ(stream_status1->code, stream_status.status().code());
  EXPECT_EQ(stream_status1->message, stream_status.status().message());

  EXPECT_EQ(state2, mojom::ParsingState::kClosed);
  EXPECT_THAT(messages2, testing::IsEmpty());
  ASSERT_TRUE(stream_status2.is_null());
}

TEST_F(TachyonParsingServiceTest, MalformedMessage) {
  TachyonParsingService service(parsing_remote_.BindNewPipeAndPassReceiver());
  BabelOrcaMessage babel_orca_message_proto = CreateBabelOrcaMessage(
      /*session_id=*/"session_id1", /*init_timestamp_ms=*/123456,
      /*order=*/5);
  ReceiveMessagesResponse received_message =
      CreateReceivedMessage(babel_orca_message_proto);
  StreamBody stream_body;
  stream_body.add_messages(received_message.SerializeAsString());
  std::string stream_string = stream_body.SerializeAsString();

  // Concat 2 which represents an invalid field number.
  parsing_remote_->Parse(base::StrCat({stream_string, std::string({2})}),
                         parse_future_.GetCallback());
  auto [state1, messages1, stream_status1] = parse_future_.Take();
  // New calls should not parse and should keep returning `kError`.
  parsing_remote_->Parse(stream_string, parse_future_.GetCallback());
  auto [state2, messages2, stream_status2] = parse_future_.Take();

  EXPECT_EQ(state1, mojom::ParsingState::kError);
  ASSERT_THAT(messages1, testing::SizeIs(1));
  EXPECT_TRUE(stream_status1.is_null());

  EXPECT_EQ(state2, mojom::ParsingState::kError);
  EXPECT_THAT(messages2, testing::IsEmpty());
  EXPECT_TRUE(stream_status2.is_null());
}

TEST_F(TachyonParsingServiceTest, InvalidReceiveMessagesResponse) {
  TachyonParsingService service(parsing_remote_.BindNewPipeAndPassReceiver());
  StreamBody stream_body_invalid;
  stream_body_invalid.add_messages("Invalid Message");
  BabelOrcaMessage babel_orca_message_proto = CreateBabelOrcaMessage(
      /*session_id=*/"session_id1", /*init_timestamp_ms=*/123456,
      /*order=*/5);
  ReceiveMessagesResponse received_message =
      CreateReceivedMessage(babel_orca_message_proto);
  StreamBody stream_body;
  stream_body.add_messages(received_message.SerializeAsString());

  parsing_remote_->Parse(stream_body_invalid.SerializeAsString(),
                         parse_future_.GetCallback());
  auto [state1, messages1, stream_status1] = parse_future_.Take();

  // New calls should not parse and should keep returning `kError`.
  parsing_remote_->Parse(stream_body.SerializeAsString(),
                         parse_future_.GetCallback());
  auto [state2, messages2, stream_status2] = parse_future_.Take();

  EXPECT_EQ(state1, mojom::ParsingState::kError);
  EXPECT_THAT(messages1, testing::IsEmpty());
  EXPECT_TRUE(stream_status1.is_null());

  EXPECT_EQ(state2, mojom::ParsingState::kError);
  EXPECT_THAT(messages2, testing::IsEmpty());
  EXPECT_TRUE(stream_status2.is_null());
}

TEST_F(TachyonParsingServiceTest, PongMessage) {
  TachyonParsingService service(parsing_remote_.BindNewPipeAndPassReceiver());
  ReceiveMessagesResponse received_message;
  received_message.mutable_pong();
  StreamBody stream_body;
  stream_body.add_messages(received_message.SerializeAsString());

  parsing_remote_->Parse(stream_body.SerializeAsString(),
                         parse_future_.GetCallback());
  auto [state, messages, stream_status] = parse_future_.Take();

  EXPECT_EQ(state, mojom::ParsingState::kOk);
  EXPECT_THAT(messages, testing::IsEmpty());
  EXPECT_TRUE(stream_status.is_null());
}

TEST_F(TachyonParsingServiceTest, NonBabelOrcaMessage) {
  TachyonParsingService service(parsing_remote_.BindNewPipeAndPassReceiver());
  ReceiveMessagesResponse received_message;
  received_message.mutable_inbox_message()->set_message("NonBabelOrca");
  StreamBody stream_body;
  stream_body.add_messages(received_message.SerializeAsString());

  parsing_remote_->Parse(stream_body.SerializeAsString(),
                         parse_future_.GetCallback());
  auto [state, messages, stream_status] = parse_future_.Take();

  EXPECT_EQ(state, mojom::ParsingState::kOk);
  EXPECT_THAT(messages, testing::IsEmpty());
  EXPECT_TRUE(stream_status.is_null());
}

TEST_F(TachyonParsingServiceTest, NoSenderEmailId) {
  TachyonParsingService service(parsing_remote_.BindNewPipeAndPassReceiver());
  BabelOrcaMessage babel_orca_message_proto = CreateBabelOrcaMessage(
      /*session_id=*/"session_id", /*init_timestamp_ms=*/123456, /*order=*/5);
  ReceiveMessagesResponse received_message =
      CreateReceivedMessage(babel_orca_message_proto);
  received_message.mutable_inbox_message()->mutable_sender_id()->set_type(
      IdType::GROUP_ID);
  StreamBody stream_body;
  stream_body.add_messages(received_message.SerializeAsString());

  parsing_remote_->Parse(stream_body.SerializeAsString(),
                         parse_future_.GetCallback());
  auto [state, messages, stream_status] = parse_future_.Take();

  EXPECT_EQ(state, mojom::ParsingState::kOk);
  EXPECT_TRUE(stream_status.is_null());
  ASSERT_THAT(messages, testing::SizeIs(1));
  VerifyBabelOrcaMessage(messages[0], babel_orca_message_proto);
  VerifyTranscriptPart(messages[0]->current_transcript,
                       babel_orca_message_proto.current_transcript());
  ASSERT_FALSE(messages[0]->previous_transcript.is_null());
  VerifyTranscriptPart(messages[0]->previous_transcript,
                       babel_orca_message_proto.previous_transcript());
  EXPECT_FALSE(messages[0]->sender_email.has_value());
}

}  // namespace
}  // namespace ash::babelorca

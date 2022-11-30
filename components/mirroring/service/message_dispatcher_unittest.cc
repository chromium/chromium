// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/message_dispatcher.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
using ::testing::InvokeWithoutArgs;
using ::testing::_;
using mirroring::mojom::CastMessage;
using mirroring::mojom::CastMessagePtr;

namespace mirroring {

namespace {

constexpr char kValidAnswerResponse[] = R"(
         { "type": "ANSWER",
            "seqNum": 12345,
            "result": "ok",
            "answer":{
              "udpPort": 50691,
              "sendIndexes": [1, 2],
              "ssrcs": [3, 4]
            }
          })";

constexpr char kValidCapabilitiesResponse[] = R"({
  "capabilities": {
    "keySystems": [],
    "mediaCaps": ["video", "h264", "vp8", "hevc", "vp9", "audio", "aac", "opus"]
  },
  "result": "ok",
  "seqNum": 820263770,
  "type": "CAPABILITIES_RESPONSE"
})";

bool IsEqual(const CastMessage& message1, const CastMessage& message2) {
  return message1.message_namespace == message2.message_namespace &&
         message1.json_format_data == message2.json_format_data;
}

}  // namespace

class MessageDispatcherTest : public mojom::CastMessageChannel,
                              public ::testing::Test {
 public:
  MessageDispatcherTest() {
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel;
    receiver_.Bind(outbound_channel.InitWithNewPipeAndPassReceiver());
    message_dispatcher_ = std::make_unique<MessageDispatcher>(
        std::move(outbound_channel),
        inbound_channel_.BindNewPipeAndPassReceiver(),
        base::BindRepeating(&MessageDispatcherTest::OnParsingError,
                            base::Unretained(this)));
    message_dispatcher_->Subscribe(
        ResponseType::ANSWER,
        base::BindRepeating(&MessageDispatcherTest::OnAnswerResponse,
                            base::Unretained(this)));
    message_dispatcher_->Subscribe(
        ResponseType::RPC,
        base::BindRepeating(&MessageDispatcherTest::OnRpcMessage,
                            base::Unretained(this)));
  }

  MessageDispatcherTest(const MessageDispatcherTest&) = delete;
  MessageDispatcherTest& operator=(const MessageDispatcherTest&) = delete;

  ~MessageDispatcherTest() override { task_environment_.RunUntilIdle(); }

  void OnParsingError(const std::string& error_message) {
    last_error_message_ = error_message;
  }

  void OnAnswerResponse(const ReceiverResponse& response) {
    last_answer_response_ = response.CloneForTesting();
  }

  void OnRpcMessage(const ReceiverResponse& response) {
    last_rpc_ = response.CloneForTesting();
  }

 protected:
  // mojom::CastMessageChannel implementation (outbound messages).
  void OnMessage(mojom::CastMessagePtr message) override {
    last_outbound_message_.message_namespace = message->message_namespace;
    last_outbound_message_.json_format_data = message->json_format_data;
  }

  // Simulates receiving an inbound message from receiver.
  void OnInboundMessage(const mojom::CastMessage& message) {
    inbound_channel_->OnMessage(message.Clone());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MessageDispatcher> message_dispatcher_;
  CastMessage last_outbound_message_;
  std::string last_error_message_;
  std::unique_ptr<ReceiverResponse> last_answer_response_;
  std::unique_ptr<ReceiverResponse> last_rpc_;

 private:
  mojo::Receiver<mojom::CastMessageChannel> receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;
};

TEST_F(MessageDispatcherTest, SendsOutboundMessage) {
  const std::string test1 = "{\"a\": 1, \"b\": 2}";
  const CastMessage message1 = CastMessage{mojom::kWebRtcNamespace, test1};
  message_dispatcher_->SendOutboundMessage(message1.Clone());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsEqual(message1, last_outbound_message_));
  EXPECT_TRUE(last_error_message_.empty());

  const std::string test2 = "{\"m\": 99, \"i\": 98, \"u\": 97}";
  const CastMessage message2 = CastMessage{mojom::kWebRtcNamespace, test2};
  message_dispatcher_->SendOutboundMessage(message2.Clone());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsEqual(message2, last_outbound_message_));
  EXPECT_TRUE(last_error_message_.empty());
}

TEST_F(MessageDispatcherTest, DispatchMessageToSubscriber) {
  // Simulate a receiver ANSWER response and expect that just the ANSWER
  // subscriber processes the message.
  const CastMessage answer_message =
      CastMessage{mojom::kWebRtcNamespace, kValidAnswerResponse};
  OnInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  EXPECT_EQ(12345, last_answer_response_->sequence_number());
  EXPECT_EQ(ResponseType::ANSWER, last_answer_response_->type());
  ASSERT_TRUE(last_answer_response_->valid());
  EXPECT_EQ(50691, last_answer_response_->answer().udp_port);
  last_answer_response_.reset();
  EXPECT_TRUE(last_error_message_.empty());

  // Simulate a receiver RPC and expect that just the
  // RPC subscriber processes the message.
  const std::string message = "Hello from the Cast Receiver!";
  std::string rpc_base64;
  base::Base64Encode(message, &rpc_base64);
  const std::string rpc =
      R"({"sessionId": 735189,
          "seqNum": 6789,
          "type": "RPC",
          "result": "ok",
          "rpc": ")" +
      rpc_base64 + R"("})";

  const CastMessage rpc_message = CastMessage{mojom::kRemotingNamespace, rpc};
  OnInboundMessage(rpc_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  ASSERT_TRUE(last_rpc_);
  EXPECT_EQ(6789, last_rpc_->sequence_number());
  EXPECT_EQ(ResponseType::RPC, last_rpc_->type());
  ASSERT_TRUE(last_rpc_->valid());
  last_rpc_.reset();
  EXPECT_TRUE(last_error_message_.empty());

  // Unsubscribe from ANSWER messages, and when feeding-in an ANSWER message,
  // nothing should happen.
  message_dispatcher_->Unsubscribe(ResponseType::ANSWER);
  OnInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  // The answer should be ignored now that we are unsubscribed.
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  EXPECT_TRUE(last_error_message_.empty());
  last_error_message_.clear();

  // However, RPC messages should still be dispatched to the
  // remaining subscriber.
  OnInboundMessage(rpc_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_TRUE(last_rpc_);
  last_rpc_.reset();
  EXPECT_TRUE(last_error_message_.empty());

  // Finally, unsubscribe from RPC messages, and when feeding-in
  // either an ANSWER or a RPC message, nothing should happen.
  message_dispatcher_->Unsubscribe(ResponseType::RPC);
  OnInboundMessage(answer_message);

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  EXPECT_TRUE(last_error_message_.empty());
  last_error_message_.clear();
  OnInboundMessage(rpc_message);

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  EXPECT_TRUE(last_error_message_.empty());
}

TEST_F(MessageDispatcherTest, IgnoreMalformedMessage) {
  const CastMessage message =
      CastMessage{mojom::kWebRtcNamespace, "MUAHAHAHAHAHAHAHA!"};
  OnInboundMessage(message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  EXPECT_FALSE(last_error_message_.empty());
}

TEST_F(MessageDispatcherTest, IgnoreMessageWithWrongNamespace) {
  const CastMessage answer_message =
      CastMessage{"Wrong_namespace", kValidAnswerResponse};
  OnInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  // Messages with different namespace are ignored with no error reported.
  EXPECT_TRUE(last_error_message_.empty());
}
TEST_F(MessageDispatcherTest, IgnoreMessageWithNoSubscribers) {
  const CastMessage unexpected_message{mojom::kWebRtcNamespace,
                                       kValidCapabilitiesResponse};
  OnInboundMessage(unexpected_message);
  task_environment_.RunUntilIdle();
  // Messages with no subscribers are ignored with no error reported.
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  EXPECT_TRUE(last_error_message_.empty());
}

TEST_F(MessageDispatcherTest, RequestReply) {
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  message_dispatcher_->Unsubscribe(ResponseType::ANSWER);
  task_environment_.RunUntilIdle();
  constexpr char kFakeOffer[] = "{\"type\":\"OFFER\",\"seqNum\":45623}";
  const CastMessage offer_message =
      CastMessage{mojom::kWebRtcNamespace, kFakeOffer};
  message_dispatcher_->RequestReply(
      offer_message.Clone(), ResponseType::ANSWER, 45623,
      base::Milliseconds(100),
      base::BindRepeating(&MessageDispatcherTest::OnAnswerResponse,
                          base::Unretained(this)));
  task_environment_.RunUntilIdle();
  // Received the request to send the outbound message.
  EXPECT_TRUE(IsEqual(offer_message, last_outbound_message_));

  const CastMessage wrong_answer_message{mojom::kWebRtcNamespace,
                                         kValidAnswerResponse};
  OnInboundMessage(wrong_answer_message);
  task_environment_.RunUntilIdle();
  // The answer message with mismatched sequence number is ignored.
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  EXPECT_TRUE(last_error_message_.empty());

  constexpr char kAnswerWithCorrectSeqNum[] = R"(
         { "type": "ANSWER",
            "seqNum": 45623,
            "result": "ok",
            "answer":{
              "udpPort": 50691,
              "sendIndexes": [1, 2],
              "ssrcs": [3, 4]
            }
          })";

  const CastMessage answer_message{mojom::kWebRtcNamespace,
                                   kAnswerWithCorrectSeqNum};
  OnInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  EXPECT_TRUE(last_error_message_.empty());
  EXPECT_EQ(45623, last_answer_response_->sequence_number());
  EXPECT_EQ(ResponseType::ANSWER, last_answer_response_->type());
  ASSERT_TRUE(last_answer_response_->valid());
  EXPECT_EQ(50691, last_answer_response_->answer().udp_port);
  last_answer_response_.reset();

  // Expect that the callback for ANSWER message was already unsubscribed.
  OnInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
  EXPECT_TRUE(last_error_message_.empty());
  last_error_message_.clear();

  const CastMessage fake_message = CastMessage{
      mojom::kWebRtcNamespace, "{\"type\":\"OFFER\",\"seqNum\":12345}"};
  message_dispatcher_->RequestReply(
      fake_message.Clone(), ResponseType::ANSWER, 12345,
      base::Milliseconds(100),
      base::BindRepeating(&MessageDispatcherTest::OnAnswerResponse,
                          base::Unretained(this)));
  task_environment_.RunUntilIdle();
  // Received the request to send the outbound message.
  EXPECT_TRUE(IsEqual(fake_message, last_outbound_message_));
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);

  // Destroy the dispatcher.
  message_dispatcher_.reset();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_rpc_);
}

}  // namespace mirroring

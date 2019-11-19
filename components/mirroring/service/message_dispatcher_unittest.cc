// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/message_dispatcher.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
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

bool IsEqual(const CastMessage& message1, const CastMessage& message2) {
  return message1.message_namespace == message2.message_namespace &&
         message1.json_format_data == message2.json_format_data;
}

void CloneResponse(const ReceiverResponse& response,
                   ReceiverResponse* cloned_response) {
  cloned_response->type = response.type;
  cloned_response->session_id = response.session_id;
  cloned_response->sequence_number = response.sequence_number;
  cloned_response->result = response.result;
  if (response.answer)
    cloned_response->answer = std::make_unique<Answer>(*response.answer);
  if (response.status)
    cloned_response->status =
        std::make_unique<ReceiverStatus>(*response.status);
  if (response.capabilities) {
    cloned_response->capabilities =
        std::make_unique<ReceiverCapability>(*response.capabilities);
  }
  cloned_response->rpc = response.rpc;
  if (response.error) {
    cloned_response->error = std::make_unique<ReceiverError>();
    cloned_response->error->code = response.error->code;
    cloned_response->error->description = response.error->description;
    cloned_response->error->details = response.error->details;
  }
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
        ResponseType::STATUS_RESPONSE,
        base::BindRepeating(&MessageDispatcherTest::OnStatusResponse,
                            base::Unretained(this)));
  }
  ~MessageDispatcherTest() override { task_environment_.RunUntilIdle(); }

  void OnParsingError(const std::string& error_message) {
    last_error_message_ = error_message;
  }

  void OnAnswerResponse(const ReceiverResponse& response) {
    if (!last_answer_response_)
      last_answer_response_ = std::make_unique<ReceiverResponse>();
    CloneResponse(response, last_answer_response_.get());
  }

  void OnStatusResponse(const ReceiverResponse& response) {
    if (!last_status_response_)
      last_status_response_ = std::make_unique<ReceiverResponse>();
    CloneResponse(response, last_status_response_.get());
  }

 protected:
  // mojom::CastMessageChannel implementation. Handles outbound message.
  void Send(mojom::CastMessagePtr message) override {
    last_outbound_message_.message_namespace = message->message_namespace;
    last_outbound_message_.json_format_data = message->json_format_data;
  }

  // Simulates receiving an inbound message from receiver.
  void SendInboundMessage(const mojom::CastMessage& message) {
    inbound_channel_->Send(message.Clone());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MessageDispatcher> message_dispatcher_;
  CastMessage last_outbound_message_;
  std::string last_error_message_;
  std::unique_ptr<ReceiverResponse> last_answer_response_;
  std::unique_ptr<ReceiverResponse> last_status_response_;

 private:
  mojo::Receiver<mojom::CastMessageChannel> receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;
  DISALLOW_COPY_AND_ASSIGN(MessageDispatcherTest);
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
  const std::string answer_response =
      "{\"type\":\"ANSWER\",\"seqNum\":12345,\"result\":\"ok\","
      "\"answer\":{\"udpPort\":50691}}";
  const CastMessage answer_message =
      CastMessage{mojom::kWebRtcNamespace, answer_response};
  SendInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  EXPECT_EQ(12345, last_answer_response_->sequence_number);
  EXPECT_EQ(ResponseType::ANSWER, last_answer_response_->type);
  EXPECT_EQ("ok", last_answer_response_->result);
  EXPECT_EQ(50691, last_answer_response_->answer->udp_port);
  EXPECT_FALSE(last_answer_response_->status);
  last_answer_response_.reset();
  EXPECT_TRUE(last_error_message_.empty());

  // Simulate a receiver STATUS_RESPONSE and expect that just the
  // STATUS_RESPONSE subscriber processes the message.
  const std::string status_response =
      "{\"type\":\"STATUS_RESPONSE\",\"seqNum\":12345,\"result\":\"ok\","
      "\"status\":{\"wifiSnr\":42}}";
  const CastMessage status_message =
      CastMessage{mojom::kWebRtcNamespace, status_response};
  SendInboundMessage(status_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  ASSERT_TRUE(last_status_response_);
  EXPECT_EQ(12345, last_status_response_->sequence_number);
  EXPECT_EQ(ResponseType::STATUS_RESPONSE, last_status_response_->type);
  EXPECT_EQ("ok", last_status_response_->result);
  EXPECT_EQ(42, last_status_response_->status->wifi_snr);
  last_status_response_.reset();
  EXPECT_TRUE(last_error_message_.empty());

  // Unsubscribe from ANSWER messages, and when feeding-in an ANSWER message,
  // nothing should happen.
  message_dispatcher_->Unsubscribe(ResponseType::ANSWER);
  SendInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  EXPECT_FALSE(last_error_message_.empty());  // Expect an error reported.
  last_error_message_.clear();

  // However, STATUS_RESPONSE messages should still be dispatcher to the
  // remaining subscriber.
  SendInboundMessage(status_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_TRUE(last_status_response_);
  last_status_response_.reset();
  EXPECT_TRUE(last_error_message_.empty());

  // Finally, unsubscribe from STATUS_RESPONSE messages, and when feeding-in
  // either an ANSWER or a STATUS_RESPONSE message, nothing should happen.
  message_dispatcher_->Unsubscribe(ResponseType::STATUS_RESPONSE);
  SendInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  EXPECT_FALSE(last_error_message_.empty());
  last_error_message_.clear();
  SendInboundMessage(status_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  EXPECT_FALSE(last_error_message_.empty());
}

TEST_F(MessageDispatcherTest, IgnoreMalformedMessage) {
  const CastMessage message =
      CastMessage{mojom::kWebRtcNamespace, "MUAHAHAHAHAHAHAHA!"};
  SendInboundMessage(message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  EXPECT_FALSE(last_error_message_.empty());
}

TEST_F(MessageDispatcherTest, IgnoreMessageWithWrongNamespace) {
  const std::string answer_response =
      "{\"type\":\"ANSWER\",\"seqNum\":12345,\"result\":\"ok\","
      "\"answer\":{\"udpPort\":50691}}";
  const CastMessage answer_message =
      CastMessage{"Wrong_namespace", answer_response};
  SendInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  // Messages with different namespace are ignored with no error reported.
  EXPECT_TRUE(last_error_message_.empty());
}

TEST_F(MessageDispatcherTest, RequestReply) {
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  message_dispatcher_->Unsubscribe(ResponseType::ANSWER);
  task_environment_.RunUntilIdle();
  const std::string fake_offer = "{\"type\":\"OFFER\",\"seqNum\":45623}";
  const CastMessage offer_message =
      CastMessage{mojom::kWebRtcNamespace, fake_offer};
  message_dispatcher_->RequestReply(
      offer_message.Clone(), ResponseType::ANSWER, 45623,
      base::TimeDelta::FromMilliseconds(100),
      base::BindRepeating(&MessageDispatcherTest::OnAnswerResponse,
                          base::Unretained(this)));
  task_environment_.RunUntilIdle();
  // Received the request to send the outbound message.
  EXPECT_TRUE(IsEqual(offer_message, last_outbound_message_));

  std::string answer_response =
      "{\"type\":\"ANSWER\",\"seqNum\":12345,\"result\":\"ok\","
      "\"answer\":{\"udpPort\":50691}}";
  CastMessage answer_message =
      CastMessage{mojom::kWebRtcNamespace, answer_response};
  SendInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  // The answer message with mismatched sequence number is ignored.
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  EXPECT_TRUE(last_error_message_.empty());

  answer_response =
      "{\"type\":\"ANSWER\",\"seqNum\":45623,\"result\":\"ok\","
      "\"answer\":{\"udpPort\":50691}}";
  answer_message = CastMessage{mojom::kWebRtcNamespace, answer_response};
  SendInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  EXPECT_TRUE(last_error_message_.empty());
  EXPECT_EQ(45623, last_answer_response_->sequence_number);
  EXPECT_EQ(ResponseType::ANSWER, last_answer_response_->type);
  EXPECT_EQ("ok", last_answer_response_->result);
  EXPECT_EQ(50691, last_answer_response_->answer->udp_port);
  last_answer_response_.reset();

  // Expect that the callback for ANSWER message was already unsubscribed.
  SendInboundMessage(answer_message);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
  EXPECT_FALSE(last_error_message_.empty());
  last_error_message_.clear();

  const CastMessage fake_message = CastMessage{
      mojom::kWebRtcNamespace, "{\"type\":\"OFFER\",\"seqNum\":12345}"};
  message_dispatcher_->RequestReply(
      fake_message.Clone(), ResponseType::ANSWER, 12345,
      base::TimeDelta::FromMilliseconds(100),
      base::BindRepeating(&MessageDispatcherTest::OnAnswerResponse,
                          base::Unretained(this)));
  task_environment_.RunUntilIdle();
  // Received the request to send the outbound message.
  EXPECT_TRUE(IsEqual(fake_message, last_outbound_message_));
  EXPECT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);

  // Destroy the dispatcher.
  message_dispatcher_.reset();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(last_answer_response_);
  EXPECT_FALSE(last_status_response_);
}

}  // namespace mirroring

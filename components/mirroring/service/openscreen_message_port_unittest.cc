// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/openscreen_message_port.h"

#include "third_party/openscreen/src/cast/common/public/message_port.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/mirroring/service/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;

namespace mirroring {
namespace {

constexpr char kSourceId[] = "sender-123";
constexpr char kDestinationId[] = "receiver-456";
constexpr char kNamespace[] = "namespace";

class MockMessagePortClient : public openscreen::cast::MessagePort::Client {
 public:
  MOCK_METHOD(void,
              OnMessage,
              (const std::string&, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, OnError, (const openscreen::Error&), (override));

  const std::string& source_id() override { return source_id_; }

 private:
  std::string source_id_ = kSourceId;
};

}  // anonymous namespace

class OpenscreenMessagePortTest : public ::testing::Test,
                                  public mojom::CastMessageChannel {
 public:
  OpenscreenMessagePortTest() = default;

  void SetUp() override {
    mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel_remote;
    outbound_channel_receiver_.Bind(
        outbound_channel_remote.InitWithNewPipeAndPassReceiver());

    message_port_ = std::make_unique<OpenscreenMessagePort>(
        kSourceId, kDestinationId, std::move(outbound_channel_remote),
        inbound_channel_.BindNewPipeAndPassReceiver());

    message_port_->SetClient(mock_client_);
    task_environment_.RunUntilIdle();
  }

  ~OpenscreenMessagePortTest() override { task_environment_.RunUntilIdle(); }

 protected:
  // mojom::CastMessageChannel implementation (outbound messages).
  void OnMessage(mojom::CastMessagePtr message) override {
    EXPECT_EQ(message->message_namespace, kNamespace);
    std::optional<base::Value> value =
        base::JSONReader::Read(message->json_format_data);
    ASSERT_TRUE(value);
    std::string message_type;
    EXPECT_TRUE(GetString(*value, "type", &message_type));
    OnOutboundMessage(message_type);
  }
  MOCK_METHOD1(OnOutboundMessage, void(const std::string& message_type));

  void OnInboundMessage(const std::string& message_namespace,
                        const std::string& json_format_data) {
    mojom::CastMessagePtr message =
        mojom::CastMessage::New(message_namespace, json_format_data);
    inbound_channel_->OnMessage(std::move(message));
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  MockMessagePortClient& client() { return mock_client_; }
  OpenscreenMessagePort* message_port() { return message_port_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Receiver<mojom::CastMessageChannel> outbound_channel_receiver_{this};
  mojo::Remote<mojom::CastMessageChannel> inbound_channel_;
  testing::StrictMock<MockMessagePortClient> mock_client_;
  std::unique_ptr<OpenscreenMessagePort> message_port_;
};

TEST_F(OpenscreenMessagePortTest, CanSendMessage) {
  EXPECT_CALL(*this, OnOutboundMessage("OFFER"));
  constexpr char kMessage[] = R"({"type": "OFFER"})";
  message_port()->PostMessage(kDestinationId, kNamespace,
                              std::string(kMessage));
  task_environment().RunUntilIdle();
}

TEST_F(OpenscreenMessagePortTest, CanReceiveMessage) {
  constexpr char kMessage[] = R"({"type": "ANSWER"})";
  EXPECT_CALL(client(), OnMessage(kDestinationId, kNamespace, kMessage));

  OnInboundMessage(kNamespace, std::string(kMessage));
  task_environment().RunUntilIdle();
}

}  // namespace mirroring

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rpc_dispatcher_impl.h"

#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "components/mirroring/service/value_util.h"
#include "components/openscreen_platform/task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/common/public/message_port.h"
#include "third_party/openscreen/src/cast/streaming/message_fields.h"
#include "third_party/openscreen/src/cast/streaming/public/session_messenger.h"
#include "third_party/openscreen/src/platform/base/error.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;

namespace mirroring {
namespace {

constexpr char kSourceId[] = "sender-123";
constexpr char kReceiverId[] = "receiver-42";

class MockMessagePort : public openscreen::cast::MessagePort {
 public:
  MockMessagePort() = default;
  ~MockMessagePort() override = default;

  MOCK_METHOD(void, SetClient, (openscreen::cast::MessagePort::Client&), ());
  MOCK_METHOD(void, ResetClient, (), ());
  MOCK_METHOD(void,
              PostMessageMock,
              (const std::string&, const std::string&, const std::string&),
              ());

  void PostMessage(const std::string& destination_sender_id,
                   const std::string& message_namespace,
                   const std::string& message) override {
    posted_messages_.push_back(message);
    PostMessageMock(destination_sender_id, message_namespace, message);
  }

  const std::vector<std::string>& posted_messages() const {
    return posted_messages_;
  }

 private:
  std::vector<std::string> posted_messages_;
};

}  // namespace

class RpcDispatcherImplTest : public ::testing::Test {
 public:
  RpcDispatcherImplTest()
      : task_environment_runner_(task_environment_.GetMainThreadTaskRunner()),
        messenger_(
            mock_message_port_,
            kSourceId,
            kReceiverId,
            [this](openscreen::Error error) { OnMessengerError(error); },
            task_environment_runner_),
        dispatcher_(messenger_) {}

  ~RpcDispatcherImplTest() override { task_environment_.RunUntilIdle(); }

  MOCK_METHOD(void, OnMessengerError, (openscreen::Error), ());
  MOCK_METHOD(void, OnMessage, (const std::vector<uint8_t>&), ());

 protected:
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  RpcDispatcherImpl& dispatcher() { return dispatcher_; }

  MockMessagePort& message_port() { return mock_message_port_; }

 private:
  base::test::TaskEnvironment task_environment_;
  openscreen_platform::TaskRunner task_environment_runner_;
  testing::NiceMock<MockMessagePort> mock_message_port_;
  openscreen::cast::SenderSessionMessenger messenger_;
  RpcDispatcherImpl dispatcher_;
};

TEST_F(RpcDispatcherImplTest, ReceivesMessages) {
  static const openscreen::cast::ReceiverMessage kMessage{
      .type = openscreen::cast::ReceiverMessage::Type::kRpc,
      .sequence_number = -1,
      .valid = true,
      .body = std::vector<uint8_t>{1, 2, 3, 4}};

  // Before we subscribe, messages should be ignored.
  EXPECT_CALL(*this, OnMessage(_)).Times(0);
  dispatcher().OnMessage(kMessage);

  EXPECT_CALL(*this, OnMessage(testing::ElementsAre(1, 2, 3, 4)));
  dispatcher().Subscribe(base::BindRepeating(
      &RpcDispatcherImplTest::OnMessage, base::Unretained(this)));
  dispatcher().OnMessage(kMessage);
}

TEST_F(RpcDispatcherImplTest, SendsMessages) {
  static const std::vector<uint8_t> kMessage{1, 2, 3, 4};

  EXPECT_CALL(message_port(),
              PostMessageMock(kReceiverId,
                              openscreen::cast::kCastRemotingNamespace, _));
  EXPECT_TRUE(dispatcher().SendOutboundMessage(kMessage));

  EXPECT_EQ(1u, message_port().posted_messages().size());
  std::optional<base::Value> value =
      base::JSONReader::Read(message_port().posted_messages()[0]);
  ASSERT_TRUE(value);

  std::string message_type;
  EXPECT_TRUE(GetString(*value, "type", &message_type));
  EXPECT_EQ("RPC", message_type);

  std::string message_binary;
  EXPECT_TRUE(GetString(*value, "rpc", &message_binary));
  EXPECT_EQ("AQIDBA==", message_binary);
}

}  // namespace mirroring

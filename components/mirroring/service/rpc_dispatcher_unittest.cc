// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rpc_dispatcher.h"

#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
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

class RpcDispatcherTest : public ::testing::Test {
 public:
  RpcDispatcherTest()
      : task_environment_runner_(task_environment_.GetMainThreadTaskRunner()),
        messenger_(
            mock_message_port_,
            kSourceId,
            kReceiverId,
            [this](openscreen::Error error) { OnMessengerError(error); },
            task_environment_runner_),
        dispatcher_(messenger_) {}

  ~RpcDispatcherTest() override = default;

  MOCK_METHOD(void, OnMessengerError, (openscreen::Error), ());
  MOCK_METHOD(void, OnMessage, (const std::vector<uint8_t>&), ());
  MOCK_METHOD(void, OnError, (), ());

  openscreen::cast::SenderSessionMessenger& messenger() { return messenger_; }

 protected:
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  RpcDispatcher& dispatcher() { return dispatcher_; }

  MockMessagePort& message_port() { return mock_message_port_; }

 private:
  base::test::TaskEnvironment task_environment_;
  openscreen_platform::TaskRunner task_environment_runner_;
  testing::NiceMock<MockMessagePort> mock_message_port_;
  openscreen::cast::SenderSessionMessenger messenger_;
  RpcDispatcher dispatcher_;
};

TEST_F(RpcDispatcherTest, ReceivesMessages) {
  // Before we subscribe, messages should be ignored.
  EXPECT_CALL(*this, OnMessage(_)).Times(0);
  openscreen::cast::MessagePort::Client& client = messenger();
  client.OnMessage(kReceiverId, openscreen::cast::kCastRemotingNamespace,
                   "{\"type\":\"RPC\",\"rpc\":\"AQIDBA==\"}");

  EXPECT_CALL(*this, OnMessage(testing::ElementsAre(1, 2, 3, 4)));
  dispatcher().Subscribe(
      base::BindRepeating(&RpcDispatcherTest::OnMessage,
                          base::Unretained(this)),
      base::BindRepeating(&RpcDispatcherTest::OnError, base::Unretained(this)));
  client.OnMessage(kReceiverId, openscreen::cast::kCastRemotingNamespace,
                   "{\"type\":\"RPC\",\"rpc\":\"AQIDBA==\"}");
}

TEST_F(RpcDispatcherTest, SendsMessages) {
  static const std::vector<uint8_t> kMessage{1, 2, 3, 4};

  EXPECT_CALL(message_port(),
              PostMessageMock(kReceiverId,
                              openscreen::cast::kCastRemotingNamespace, _));
  EXPECT_TRUE(dispatcher().SendOutboundMessage(kMessage));

  EXPECT_EQ(1u, message_port().posted_messages().size());
  std::optional<base::Value> value =
      base::JSONReader::Read(message_port().posted_messages()[0],
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(value);

  ASSERT_TRUE(value->is_dict());
  const std::string* message_type = value->GetDict().FindString("type");
  ASSERT_TRUE(message_type);
  EXPECT_EQ("RPC", *message_type);

  const std::string* message_binary = value->GetDict().FindString("rpc");
  ASSERT_TRUE(message_binary);
  EXPECT_EQ("AQIDBA==", *message_binary);
}

TEST_F(RpcDispatcherTest, DefaultConstructedDispatcherFailsGracefully) {
  RpcDispatcher default_dispatcher;
  static const std::vector<uint8_t> kMessage{1, 2, 3, 4};
  EXPECT_FALSE(default_dispatcher.SendOutboundMessage(kMessage));

  default_dispatcher.Subscribe(
      base::BindRepeating(&RpcDispatcherTest::OnMessage,
                          base::Unretained(this)),
      base::BindRepeating(&RpcDispatcherTest::OnError, base::Unretained(this)));
  default_dispatcher.Unsubscribe();
}

TEST_F(RpcDispatcherTest, UnsubscribeStopsDeliveringMessages) {
  dispatcher().Subscribe(
      base::BindRepeating(&RpcDispatcherTest::OnMessage,
                          base::Unretained(this)),
      base::BindRepeating(&RpcDispatcherTest::OnError, base::Unretained(this)));
  dispatcher().Unsubscribe();

  EXPECT_CALL(*this, OnMessage(_)).Times(0);
  EXPECT_CALL(*this, OnError()).Times(0);

  openscreen::cast::MessagePort::Client& client = messenger();
  client.OnMessage(kReceiverId, openscreen::cast::kCastRemotingNamespace,
                   "{\"type\":\"RPC\",\"rpc\":\"AQIDBA==\"}");
}

TEST_F(RpcDispatcherTest, CallbackCanUnsubscribeWithoutCrashing) {
  dispatcher().Subscribe(
      base::BindRepeating(
          [](RpcDispatcher* dispatcher, const std::vector<uint8_t>&) {
            dispatcher->Unsubscribe();
          },
          &dispatcher()),
      base::BindRepeating(&RpcDispatcherTest::OnError, base::Unretained(this)));

  openscreen::cast::MessagePort::Client& client = messenger();
  client.OnMessage(kReceiverId, openscreen::cast::kCastRemotingNamespace,
                   "{\"type\":\"RPC\",\"rpc\":\"AQIDBA==\"}");
}

TEST_F(RpcDispatcherTest, DestructionSafelyUnregistersHandler) {
  auto local_dispatcher = std::make_unique<RpcDispatcher>(messenger());
  EXPECT_CALL(*this, OnMessage(_)).Times(0);
  EXPECT_CALL(*this, OnError()).Times(0);
  local_dispatcher->Subscribe(
      base::BindRepeating(&RpcDispatcherTest::OnMessage,
                          base::Unretained(this)),
      base::BindRepeating(&RpcDispatcherTest::OnError, base::Unretained(this)));

  local_dispatcher.reset();

  openscreen::cast::MessagePort::Client& client = messenger();
  client.OnMessage(kReceiverId, openscreen::cast::kCastRemotingNamespace,
                   "{\"type\":\"RPC\",\"rpc\":\"AQIDBA==\"}");
}

}  // namespace mirroring

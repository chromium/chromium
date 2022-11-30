// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/cast/message_port/blink_message_port_adapter.h"
#include "components/cast/message_port/cast/message_port_cast.h"
#include "components/cast/message_port/cast_core/create_message_port_core.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast/message_port/message_port_buildflags.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"
#endif  // BUILDFLAG(IS_FUCHSIA)

#ifdef PostMessage
#undef PostMessage
#endif

namespace cast_api_bindings {

using CreatePairFunction = void (*)(std::unique_ptr<MessagePort>*,
                                    std::unique_ptr<MessagePort>*);

// Creates a PlatformMessagePort |client | talking to MessagePortCast |server|
static void CreatePlatformToBlinkPair(std::unique_ptr<MessagePort>* client,
                                      std::unique_ptr<MessagePort>* server) {
  std::unique_ptr<MessagePort> server_adapter;
  MessagePortCast::CreatePair(&server_adapter, server);
  *client = BlinkMessagePortAdapter::ToClientPlatformMessagePort(
      MessagePortCast::FromMessagePort(server_adapter.get())->TakePort());
}

// Creates a MessagePortCast |client | talking to PlatformMessagePort |server|
static void CreateBlinkToPlatformPair(std::unique_ptr<MessagePort>* client,
                                      std::unique_ptr<MessagePort>* server) {
  std::unique_ptr<MessagePort> server_adapter;
  CreatePlatformMessagePortPair(&server_adapter, server);
  *client = std::make_unique<MessagePortCast>(
      BlinkMessagePortAdapter::FromServerPlatformMessagePort(
          std::move(server_adapter)));
}

class MessagePortTest : public ::testing::Test {
 public:
  MessagePortTest() : MessagePortTest(&CreatePlatformMessagePortPair) {}

  // Allows parameterized tests to modify which ports are created.
  explicit MessagePortTest(CreatePairFunction create_pair)
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        create_pair_(create_pair) {
    CreatePair(&client_, &server_);
  }

  ~MessagePortTest() override = default;

  void CreatePair(std::unique_ptr<MessagePort>* client,
                  std::unique_ptr<MessagePort>* server) {
    create_pair_(client, server);
  }

  void SetDefaultReceivers() {
    client_->SetReceiver(&client_receiver_);
    server_->SetReceiver(&server_receiver_);
  }

  // Posts multiple |messages| from |sender| to |receiver| and validates their
  // arrival order
  void PostMessages(const std::vector<std::string>& messages,
                    MessagePort* sender,
                    TestMessagePortReceiver* receiver) {
    for (const auto& message : messages) {
      sender->PostMessage(message);
    }

    EXPECT_TRUE(receiver->RunUntilMessageCountEqual(messages.size()));
    for (size_t i = 0; i < messages.size(); i++) {
      EXPECT_EQ(receiver->buffer()[i].first, messages[i]);
    }
  }

  // Posts a |port| from |sender| to |receiver| and validates its arrival.
  // Returns the transferred |port|.
  std::unique_ptr<MessagePort> PostMessageWithTransferables(
      std::unique_ptr<MessagePort> port,
      MessagePort* sender,
      TestMessagePortReceiver* receiver) {
    std::vector<std::unique_ptr<MessagePort>> ports;
    ports.emplace_back(std::move(port));
    sender->PostMessageWithTransferables("", std::move(ports));
    EXPECT_TRUE(receiver->RunUntilMessageCountEqual(1));
    EXPECT_EQ(receiver->buffer()[0].second.size(), (size_t)1);
    return std::move(receiver->buffer()[0].second[0]);
  }

  void TestPostMessage() {
    SetDefaultReceivers();
    PostMessages({"from client"}, client_.get(), &server_receiver_);
    PostMessages({"from server"}, server_.get(), &client_receiver_);
  }

 protected:
  std::unique_ptr<MessagePort> client_;
  std::unique_ptr<MessagePort> server_;
  TestMessagePortReceiver client_receiver_;
  TestMessagePortReceiver server_receiver_;

 private:
  const base::test::TaskEnvironment task_environment_;
  CreatePairFunction create_pair_;
};

TEST_F(MessagePortTest, WrapPlatformPort) {
  // Initialize ports from the platform type instead of agnostic CreatePair
#if BUILDFLAG(USE_MESSAGE_PORT_CORE)
  cast_api_bindings::CreateMessagePortCorePair(&client_, &server_);
#elif BUILDFLAG(IS_FUCHSIA)
  fidl::InterfaceHandle<fuchsia::web::MessagePort> port0;
  fidl::InterfaceRequest<fuchsia::web::MessagePort> port1 = port0.NewRequest();
  client_ = MessagePortFuchsia::Create(std::move(port0));
  server_ = MessagePortFuchsia::Create(std::move(port1));
#else
  auto pair = blink::WebMessagePort::CreatePair();
  client_ = MessagePortCast::Create(std::move(pair.first));
  server_ = MessagePortCast::Create(std::move(pair.second));
#endif  // BUILDFLAG(IS_FUCHSIA)

  TestPostMessage();
}

// Test unwrapping via TakePort (rewrapped for test methods)
TEST_F(MessagePortTest, UnwrapPlatformPort) {
  // Workaround for parameterized tests which would create the
  // wrong port type
  CreatePlatformMessagePortPair(&client_, &server_);
#if BUILDFLAG(USE_MESSAGE_PORT_CORE)
  client_.reset(
      cast_api_bindings::MessagePortCore::FromMessagePort(client_.release()));
  server_.reset(
      cast_api_bindings::MessagePortCore::FromMessagePort(server_.release()));
#elif BUILDFLAG(IS_FUCHSIA)
  client_ = MessagePortFuchsia::Create(
      MessagePortFuchsia::FromMessagePort(client_.get())->TakeClientHandle());
  server_ = MessagePortFuchsia::Create(
      MessagePortFuchsia::FromMessagePort(server_.get())->TakeServiceRequest());
#else
  client_ = MessagePortCast::Create(
      MessagePortCast::FromMessagePort(client_.get())->TakePort());
  server_ = MessagePortCast::Create(
      MessagePortCast::FromMessagePort(server_.get())->TakePort());
#endif  // BUILDFLAG(IS_FUCHSIA)

  TestPostMessage();
}

enum MessagePortTestType {
  PLATFORM,
  PLATFORM_TO_BLINK,
  BLINK_TO_PLATFORM,
  FUCHSIA,
  CORE,
  CAST
};

struct MessagePortTestParam {
  MessagePortTestType type;
  CreatePairFunction func;
};

const MessagePortTestParam MessagePortTestParams[] = {
    {MessagePortTestType::PLATFORM, &CreatePlatformMessagePortPair},
    {MessagePortTestType::PLATFORM_TO_BLINK, &CreatePlatformToBlinkPair},
    {MessagePortTestType::BLINK_TO_PLATFORM, &CreateBlinkToPlatformPair},
#if BUILDFLAG(IS_FUCHSIA)
    {MessagePortTestType::FUCHSIA, &MessagePortFuchsia::CreatePair},
#endif  // BUILDFLAG(IS_FUCHSIA)
    {MessagePortTestType::CORE, &CreateMessagePortCorePair},
    {MessagePortTestType::CAST, &MessagePortCast::CreatePair}};

class ParameterizedMessagePortTest
    : public MessagePortTest,
      public ::testing::WithParamInterface<MessagePortTestParam> {
 public:
  ParameterizedMessagePortTest() : MessagePortTest(GetParam().func) {}
  ~ParameterizedMessagePortTest() override = default;
};

// Run the tests on all port types supported by the platform.
INSTANTIATE_TEST_SUITE_P(ParameterizedMessagePortTest,
                         ParameterizedMessagePortTest,
                         testing::ValuesIn(MessagePortTestParams));

TEST_P(ParameterizedMessagePortTest, Close) {
  SetDefaultReceivers();
  ASSERT_TRUE(client_->CanPostMessage());
  ASSERT_TRUE(server_->CanPostMessage());

  server_->Close();

  // cast_api_bindings::MessagePort reports closure PostMessage is attempted,
  // but other ports report it proactively
  client_->PostMessage("");
  client_receiver_.RunUntilDisconnected();
  ASSERT_FALSE(client_->CanPostMessage());
  ASSERT_FALSE(server_->CanPostMessage());
}

TEST_P(ParameterizedMessagePortTest, OnError) {
  server_receiver_.SetOnMessageResult(false);
  SetDefaultReceivers();
  client_->PostMessage("");

#if BUILDFLAG(IS_FUCHSIA)
  // blink::WebMessagePort reports failure when PostMessage returns false, but
  // fuchsia::web::MessagePort will not report the error until the port closes
  server_receiver_.RunUntilMessageCountEqual(1);
  server_.reset();
#endif

  client_receiver_.RunUntilDisconnected();
}

TEST_P(ParameterizedMessagePortTest, OnErrorOnClose) {
  SetDefaultReceivers();
  server_.reset();
  client_receiver_.RunUntilDisconnected();
}

TEST_P(ParameterizedMessagePortTest, PostMessage) {
  TestPostMessage();
}

TEST_P(ParameterizedMessagePortTest, PostMessageMultiple) {
  SetDefaultReceivers();
  PostMessages({"c1", "c2", "c3"}, client_.get(), &server_receiver_);
  PostMessages({"s1", "s2", "s3"}, server_.get(), &client_receiver_);
}

TEST_P(ParameterizedMessagePortTest, PostMessageWithTransferables) {
  std::unique_ptr<MessagePort> port0;
  std::unique_ptr<MessagePort> port1;
  TestMessagePortReceiver port0_receiver;
  TestMessagePortReceiver port1_receiver;

  // For the adapter tests, the ports being passed are inverted:
  // - CreateBlinkToPlatformPair gives a client blink and server platform port,
  //   but port0 needs to be server blink and port1 client platform
  // - CreatePlatformToBlinkPair gives a client platform and server blink port,
  //   but port0 needs to be server platform and port1 client blink
  MessagePortTestType t = GetParam().type;
  if (t == MessagePortTestType::BLINK_TO_PLATFORM) {
    CreatePlatformToBlinkPair(&port1, &port0);
  } else if (t == MessagePortTestType::PLATFORM_TO_BLINK) {
    CreateBlinkToPlatformPair(&port1, &port0);
  } else {
    CreatePair(&port1, &port0);
  }

  // If the ports are represented by multiple types as in the case of
  // MessagePortFuchsia, make sure both are transferrable
  SetDefaultReceivers();
  port0 = PostMessageWithTransferables(std::move(port0), client_.get(),
                                       &server_receiver_);
  port1 = PostMessageWithTransferables(std::move(port1), server_.get(),
                                       &client_receiver_);

  // Make sure the ports are still usable
  port0->SetReceiver(&port0_receiver);
  port1->SetReceiver(&port1_receiver);
  PostMessages({"from port0"}, port0.get(), &port1_receiver);
  PostMessages({"from port1"}, port1.get(), &port0_receiver);
}

}  // namespace cast_api_bindings

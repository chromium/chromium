// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/message_port/message_port.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_FUCHSIA)
#include "components/cast/message_port/message_port_fuchsia.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#else
#include "components/cast/message_port/message_port_cast.h"  // nogncheck
#include "third_party/blink/public/common/messaging/web_message_port.h"  // nogncheck
#endif  // defined(OS_FUCHSIA)

#ifdef PostMessage
#undef PostMessage
#endif

namespace cast_api_bindings {

class MessagePortTest : public ::testing::Test {
 public:
  MessagePortTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    MessagePort::CreatePair(&client_, &server_);
  }

  ~MessagePortTest() override = default;

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
};

TEST_F(MessagePortTest, Close) {
  SetDefaultReceivers();
  ASSERT_TRUE(client_->CanPostMessage());
  ASSERT_TRUE(server_->CanPostMessage());

  server_->Close();
  client_receiver_.RunUntilDisconnected();
  ASSERT_FALSE(client_->CanPostMessage());
  ASSERT_FALSE(server_->CanPostMessage());
}

TEST_F(MessagePortTest, OnError) {
  server_receiver_.SetOnMessageResult(false);
  SetDefaultReceivers();
  client_->PostMessage("");

#if defined(OS_FUCHSIA)
  // blink::WebMessagePort reports failure when PostMessage returns false, but
  // fuchsia::web::MessagePort will not report the error until the port closes
  server_receiver_.RunUntilMessageCountEqual(1);
  server_.reset();
#endif

  client_receiver_.RunUntilDisconnected();
}

TEST_F(MessagePortTest, PostMessage) {
  TestPostMessage();
}

TEST_F(MessagePortTest, PostMessageMultiple) {
  SetDefaultReceivers();
  PostMessages({"c1", "c2", "c3"}, client_.get(), &server_receiver_);
  PostMessages({"s1", "s2", "s3"}, server_.get(), &client_receiver_);
}

TEST_F(MessagePortTest, PostMessageWithTransferables) {
  std::unique_ptr<MessagePort> port0;
  std::unique_ptr<MessagePort> port1;
  TestMessagePortReceiver port0_receiver;
  TestMessagePortReceiver port1_receiver;
  MessagePort::CreatePair(&port0, &port1);

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

TEST_F(MessagePortTest, WrapPlatformPort) {
  // Initialize ports from the platform type instead of agnostic CreatePair
#if defined(OS_FUCHSIA)
  fidl::InterfaceHandle<fuchsia::web::MessagePort> port0;
  fidl::InterfaceRequest<fuchsia::web::MessagePort> port1 = port0.NewRequest();
  client_ = MessagePortFuchsia::Create(std::move(port0));
  server_ = MessagePortFuchsia::Create(std::move(port1));
#else
  auto pair = blink::WebMessagePort::CreatePair();
  client_ = MessagePortCast::Create(std::move(pair.first));
  server_ = MessagePortCast::Create(std::move(pair.second));
#endif  // defined(OS_FUCHSIA)

  TestPostMessage();
}

TEST_F(MessagePortTest, UnwrapPlatformPortCast) {
  // Test unwrapping via TakePort (rewrapped for test methods)
#if defined(OS_FUCHSIA)
  client_ = MessagePortFuchsia::Create(
      MessagePortFuchsia::FromMessagePort(client_.get())->TakeClientHandle());
  server_ = MessagePortFuchsia::Create(
      MessagePortFuchsia::FromMessagePort(server_.get())->TakeServiceRequest());
#else
  client_ = MessagePortCast::Create(
      MessagePortCast::FromMessagePort(client_.get())->TakePort());
  server_ = MessagePortCast::Create(
      MessagePortCast::FromMessagePort(server_.get())->TakePort());
#endif  // defined(OS_FUCHSIA)

  TestPostMessage();
}

}  // namespace cast_api_bindings

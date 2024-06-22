// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/message_port_tls_connection.h"

#include <memory>
#include <queue>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "components/cast/message_port/message_port.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/platform/api/task_runner.h"
#include "third_party/openscreen/src/platform/api/tls_connection.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"
#include "third_party/openscreen/src/platform/base/span.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

namespace openscreen_platform {
namespace {

class MockMessagePort : public cast_api_bindings::MessagePort {
 public:
  ~MockMessagePort() override = default;

  MOCK_METHOD1(PostMessage, bool(std::string_view));
  MOCK_METHOD2(PostMessageWithTransferables,
               bool(std::string_view,
                    std::vector<std::unique_ptr<MessagePort>>));
  MOCK_METHOD1(SetReceiver, void(cast_api_bindings::MessagePort::Receiver*));
  MOCK_METHOD0(Close, void());
  MOCK_CONST_METHOD0(CanPostMessage, bool());
};

class MockTlsConnectionClient : public openscreen::TlsConnection::Client {
 public:
  ~MockTlsConnectionClient() override = default;

  MOCK_METHOD2(OnRead, void(openscreen::TlsConnection*, std::vector<uint8_t>));
  MOCK_METHOD2(OnError,
               void(openscreen::TlsConnection*, const openscreen::Error&));
};

class MockTaskRunner : public openscreen::TaskRunner {
 public:
  ~MockTaskRunner() override = default;

  // openscreen::TaskRunner overrides;
  MOCK_METHOD2(PostPackagedTaskWithDelay,
               void(Task, openscreen::Clock::duration));
  MOCK_METHOD0(IsRunningOnTaskRunner, bool());

  void PostPackagedTask(Task task) override {
    tasks_.push(std::move(task));
    PostTask();
  }

  void RunTasksUntilIdle() {
    while (!tasks_.empty()) {
      tasks_.front()();
      tasks_.pop();
    }
  }

  MOCK_METHOD0(PostTask, void());

 private:
  std::queue<Task> tasks_;
};

}  // namespace

class MessagePortTlsConnectionTest : public testing::Test {
 public:
  MessagePortTlsConnectionTest() {
    auto message_port = std::make_unique<MockMessagePort>();
    message_port_ = message_port.get();

    EXPECT_CALL(*message_port_, SetReceiver(_));
    connection_ = std::make_unique<MessagePortTlsConnection>(
        std::move(message_port), task_runner_);
    connection_as_receiver_ = connection_.get();
  }
  ~MessagePortTlsConnectionTest() override = default;

 protected:
  std::unique_ptr<MessagePortTlsConnection> connection_;
  raw_ptr<cast_api_bindings::MessagePort::Receiver> connection_as_receiver_;

  raw_ptr<MockMessagePort> message_port_;
  StrictMock<MockTlsConnectionClient> client_;
  StrictMock<MockTaskRunner> task_runner_;
};

TEST_F(MessagePortTlsConnectionTest, OnMessage) {
  std::string_view message = "foo";

  // No operation done when no client is set.
  connection_as_receiver_->OnMessage(message, {});

  // Setting the client checks the Task runner.
  // NOTE: The IsRunningOnTaskRunner() call is DCHECK'd, so it will only run on
  // debug builds - so it may be called zero or one times.
  EXPECT_CALL(task_runner_, IsRunningOnTaskRunner())
      .WillRepeatedly(Return(true));
  connection_->SetClient(&client_);

  // Once the client is set, a callback is made when OnMessage*() is called on
  // the correct task runner.
  message = "bar";
  EXPECT_CALL(task_runner_, IsRunningOnTaskRunner()).WillOnce(Return(true));
  EXPECT_CALL(client_,
              OnRead(connection_.get(),
                     std::vector<uint8_t>(message.begin(), message.end())));
  connection_as_receiver_->OnMessage(message, {});

  // Once the client is set, a callback is pushed to the task runner if it's not
  // already being run from there.
  message = "foobar";
  EXPECT_CALL(task_runner_, IsRunningOnTaskRunner()).WillOnce(Return(false));
  EXPECT_CALL(task_runner_, PostTask());
  connection_as_receiver_->OnMessage(message, {});

  EXPECT_CALL(task_runner_, IsRunningOnTaskRunner()).WillOnce(Return(true));
  EXPECT_CALL(client_,
              OnRead(connection_.get(),
                     std::vector<uint8_t>(message.begin(), message.end())));
  task_runner_.RunTasksUntilIdle();
}

TEST_F(MessagePortTlsConnectionTest, OnPipeError) {
  // No operation done when no client is set.
  connection_as_receiver_->OnPipeError();

  // Setting the client checks the Task runner.
  // NOTE: The IsRunningOnTaskRunner() call is DCHECK'd, so it will only run on
  // debug builds - so it may be called zero or one times.
  EXPECT_CALL(task_runner_, IsRunningOnTaskRunner())
      .WillRepeatedly(Return(true));
  connection_->SetClient(&client_);

  // Once the client is set, a callback is made when OnPipeError() is called on
  // the correct task runner.
  EXPECT_CALL(task_runner_, IsRunningOnTaskRunner()).WillOnce(Return(true));
  EXPECT_CALL(client_, OnError(connection_.get(), _));
  connection_as_receiver_->OnPipeError();

  // Once the client is set, a callback is pushed to the task runner if it's not
  // already being run from there.
  EXPECT_CALL(task_runner_, IsRunningOnTaskRunner()).WillOnce(Return(false));
  EXPECT_CALL(task_runner_, PostTask());
  connection_as_receiver_->OnPipeError();

  EXPECT_CALL(task_runner_, IsRunningOnTaskRunner()).WillOnce(Return(true));
  EXPECT_CALL(client_, OnError(connection_.get(), _));
  task_runner_.RunTasksUntilIdle();
}

TEST_F(MessagePortTlsConnectionTest, Send) {
  const std::string_view message = "foobar";

  // Set data is always forwarded to the underlying MessagePort's Send().
  EXPECT_CALL(*message_port_, PostMessage(message));
  connection_->Send(openscreen::ByteView(
      reinterpret_cast<const uint8_t*>(message.data()), message.size()));
}

}  // namespace openscreen_platform

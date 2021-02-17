// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/keep_alive_delegate.h"

#include <stdint.h>

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/cast_channel/cast_test_util.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Sequence;

namespace cast_channel {
namespace {

const int64_t kTestPingTimeoutMillis = 1000;
const int64_t kTestLivenessTimeoutMillis = 10000;

CastMessage CreateNonKeepAliveMessage(const std::string& message_type) {
  CastMessage output;
  output.set_protocol_version(CastMessage::CASTV2_1_0);
  output.set_source_id("source");
  output.set_destination_id("receiver");
  output.set_namespace_("some.namespace");
  output.set_payload_type(
      CastMessage::PayloadType::CastMessage_PayloadType_STRING);

  base::DictionaryValue type_dict;
  type_dict.SetString("type", message_type);
  CHECK(base::JSONWriter::Write(type_dict, output.mutable_payload_utf8()));
  return output;
}

// Extends MockTimer with a mockable method ResetTriggered() which permits
// test code to set GMock expectations for Timer::Reset().
class MockTimerWithMonitoredReset : public base::MockRetainingOneShotTimer {
 public:
  MockTimerWithMonitoredReset() {}
  ~MockTimerWithMonitoredReset() override {}

  // Instrumentation point for determining how many times Reset() was called.
  MOCK_METHOD0(ResetTriggered, void(void));
  MOCK_METHOD0(StopTriggered, void(void));

  // Passes through the Reset call to the base MockTimer and visits the mock
  // ResetTriggered method.
  void Reset() override {
    base::MockRetainingOneShotTimer::Reset();
    ResetTriggered();
  }

  void Stop() override {
    base::MockRetainingOneShotTimer::Stop();
    StopTriggered();
  }
};

class KeepAliveDelegateTest : public testing::Test {
 public:
  using ChannelError = ::cast_channel::ChannelError;

  KeepAliveDelegateTest() {}
  ~KeepAliveDelegateTest() override {}

 protected:
  void SetUp() override {
    inner_delegate_ = new MockCastTransportDelegate;
    logger_ = new Logger();
    keep_alive_.reset(new KeepAliveDelegate(
        &socket_, logger_, base::WrapUnique(inner_delegate_),
        base::TimeDelta::FromMilliseconds(kTestPingTimeoutMillis),
        base::TimeDelta::FromMilliseconds(kTestLivenessTimeoutMillis)));
    liveness_timer_ = new MockTimerWithMonitoredReset;
    ping_timer_ = new MockTimerWithMonitoredReset;
    EXPECT_CALL(*liveness_timer_, StopTriggered()).Times(0);
    EXPECT_CALL(*ping_timer_, StopTriggered()).Times(0);
    keep_alive_->SetTimersForTest(base::WrapUnique(ping_timer_),
                                  base::WrapUnique(liveness_timer_));
  }

  // Runs all pending tasks in the message loop.
  void RunPendingTasks() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockCastSocket socket_;
  std::unique_ptr<KeepAliveDelegate> keep_alive_;
  scoped_refptr<Logger> logger_;
  MockCastTransportDelegate* inner_delegate_;
  MockTimerWithMonitoredReset* liveness_timer_;
  MockTimerWithMonitoredReset* ping_timer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeepAliveDelegateTest);
};

TEST_F(KeepAliveDelegateTest, TestErrorHandledBeforeStarting) {
  EXPECT_CALL(*inner_delegate_, OnError(ChannelError::CONNECT_ERROR));
  keep_alive_->OnError(ChannelError::CONNECT_ERROR);
}

TEST_F(KeepAliveDelegateTest, TestPing) {
  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(CreateKeepAlivePingMessage()), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(3);
  EXPECT_CALL(*ping_timer_, StopTriggered());

  keep_alive_->Start();
  ping_timer_->Fire();
  EXPECT_FALSE(ping_timer_->IsRunning());

  keep_alive_->OnMessage(CreateKeepAlivePongMessage());
  RunPendingTasks();
  EXPECT_TRUE(ping_timer_->IsRunning());
}

TEST_F(KeepAliveDelegateTest, TestPingFailed) {
  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(CreateKeepAlivePingMessage()), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::ERR_CONNECTION_RESET));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*inner_delegate_, OnError(ChannelError::CAST_SOCKET_ERROR));
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(1);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(1);
  EXPECT_CALL(*liveness_timer_, StopTriggered());
  EXPECT_CALL(*ping_timer_, StopTriggered()).Times(2);

  keep_alive_->Start();
  ping_timer_->Fire();
  RunPendingTasks();
  EXPECT_EQ(ChannelEvent::PING_WRITE_ERROR,
            logger_->GetLastError(socket_.id()).channel_event);
  EXPECT_EQ(net::ERR_CONNECTION_RESET,
            logger_->GetLastError(socket_.id()).net_return_value);
}

TEST_F(KeepAliveDelegateTest, TestPingAndLivenessTimeout) {
  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(CreateKeepAlivePingMessage()), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  EXPECT_CALL(*inner_delegate_, OnError(ChannelError::PING_TIMEOUT));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(1);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, StopTriggered()).Times(2);
  EXPECT_CALL(*ping_timer_, StopTriggered()).Times(2);

  keep_alive_->Start();
  ping_timer_->Fire();
  liveness_timer_->Fire();
  RunPendingTasks();
}

TEST_F(KeepAliveDelegateTest, TestResetTimersAndPassthroughAllOtherTraffic) {
  CastMessage other_message = CreateNonKeepAliveMessage("someMessageType");

  EXPECT_CALL(*inner_delegate_, OnMessage(EqualsProto(other_message)));
  EXPECT_CALL(*inner_delegate_, Start());
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(2);

  keep_alive_->Start();
  keep_alive_->OnMessage(other_message);
  RunPendingTasks();
}

TEST_F(KeepAliveDelegateTest, TestPassthroughMessagesAfterError) {
  CastMessage message = CreateNonKeepAliveMessage("someMessageType");
  CastMessage message_after_error =
      CreateNonKeepAliveMessage("someMessageType2");
  CastMessage late_ping_message = CreateKeepAlivePingMessage();

  EXPECT_CALL(*inner_delegate_, Start()).Times(1);
  EXPECT_CALL(*ping_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, ResetTriggered()).Times(2);
  EXPECT_CALL(*liveness_timer_, StopTriggered()).Times(1);
  EXPECT_CALL(*ping_timer_, StopTriggered()).Times(1);

  Sequence message_and_error_sequence;
  EXPECT_CALL(*inner_delegate_, OnMessage(EqualsProto(message)))
      .Times(1)
      .InSequence(message_and_error_sequence)
      .RetiresOnSaturation();
  EXPECT_CALL(*inner_delegate_, OnError(ChannelError::INVALID_MESSAGE))
      .Times(1)
      .InSequence(message_and_error_sequence);
  EXPECT_CALL(*inner_delegate_, OnMessage(EqualsProto(message_after_error)))
      .Times(1)
      .InSequence(message_and_error_sequence)
      .RetiresOnSaturation();
  EXPECT_CALL(*inner_delegate_, OnMessage(EqualsProto(late_ping_message)))
      .Times(0)
      .InSequence(message_and_error_sequence)
      .RetiresOnSaturation();

  // Start, process one message, then error-out. KeepAliveDelegate will
  // automatically stop itself.
  keep_alive_->Start();
  keep_alive_->OnMessage(message);
  RunPendingTasks();
  keep_alive_->OnError(ChannelError::INVALID_MESSAGE);
  RunPendingTasks();

  // Process a non-PING/PONG message and expect it to pass through.
  keep_alive_->OnMessage(message_after_error);
  RunPendingTasks();

  // Process a late-arriving PING/PONG message, which should have no effect.
  keep_alive_->OnMessage(late_ping_message);
  RunPendingTasks();
}

TEST_F(KeepAliveDelegateTest, TestLivenessTimerResetAfterSendingMessage) {
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner(
      new base::TestMockTimeTaskRunner());
  auto liveness_timer = std::make_unique<base::RetainingOneShotTimer>(
      mock_time_task_runner->GetMockTickClock());
  auto ping_timer = std::make_unique<base::RetainingOneShotTimer>(
      mock_time_task_runner->GetMockTickClock());
  ping_timer->SetTaskRunner(mock_time_task_runner);
  liveness_timer->SetTaskRunner(mock_time_task_runner);
  keep_alive_->SetTimersForTest(std::move(ping_timer),
                                std::move(liveness_timer));

  // At time 0, start.
  EXPECT_CALL(*inner_delegate_, Start());
  keep_alive_->Start();

  EXPECT_CALL(*socket_.mock_transport(),
              SendMessage(EqualsProto(CreateKeepAlivePingMessage()), _))
      .WillOnce(PostCompletionCallbackTask<1>(net::OK));
  // Forward 1s, at time 1, fire ping timer.
  mock_time_task_runner->FastForwardBy(
      base::TimeDelta::FromMilliseconds(kTestPingTimeoutMillis));

  // Forward 9s, at Time 10, do not fire liveness timer.
  EXPECT_CALL(*inner_delegate_, OnError(_)).Times(0);
  mock_time_task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(
      kTestLivenessTimeoutMillis - kTestPingTimeoutMillis));

  // Forward 1s, at time 11s, fire liveness timer.
  EXPECT_CALL(*inner_delegate_, OnError(_));
  mock_time_task_runner->FastForwardBy(
      base::TimeDelta::FromMilliseconds(kTestPingTimeoutMillis));
}

}  // namespace
}  // namespace cast_channel

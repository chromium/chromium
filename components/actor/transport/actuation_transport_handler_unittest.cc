// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/transport/actuation_transport_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/actor/transport/actuation_delegate.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

class FakeTransportSession : public browser_actuator::TransportSession {
 public:
  explicit FakeTransportSession(std::string_view session_id)
      : session_id_(session_id) {}
  ~FakeTransportSession() override = default;

  std::string_view GetSessionId() const override { return session_id_; }

  base::expected<void, browser_actuator::SendUpstreamMessageError>
  SendUpstreamMessage(browser_actuator::PayloadType payload_type,
                      const google::protobuf::MessageLite& message) override {
    sent_payload_types_.push_back(payload_type);
    proto::ActuationUpstreamMessage upstream;
    EXPECT_TRUE(upstream.ParseFromString(message.SerializeAsString()));
    sent_messages_.push_back(std::move(upstream));
    return {};
  }

  void OnMessage(browser_actuator::PayloadType payload_type,
                 const google::protobuf::MessageLite& message) override {}

  const std::vector<browser_actuator::PayloadType>& sent_payload_types() const {
    return sent_payload_types_;
  }

  const std::vector<proto::ActuationUpstreamMessage>& sent_messages() const {
    return sent_messages_;
  }

 private:
  std::string session_id_;
  std::vector<browser_actuator::PayloadType> sent_payload_types_;
  std::vector<proto::ActuationUpstreamMessage> sent_messages_;
};

class FakeActuationDelegate : public ActuationDelegate {
 public:
  FakeActuationDelegate() = default;
  ~FakeActuationDelegate() override = default;

  void StartTask(const std::string& session_id,
                 const optimization_guide::proto::BrowserStartTask& request,
                 StartTaskCallback callback) override {
    last_start_session_id_ = session_id;
    last_start_request_ = request;
    pending_start_callback_ = std::move(callback);
  }

  void StopTask(TaskId task_id, StopTaskCallback callback) override {
    last_stop_task_id_ = task_id;
    stopped_task_ids_.push_back(task_id);
    pending_stop_callback_ = std::move(callback);
  }

  void Act(const optimization_guide::proto::Actions& actions,
           ActCallback callback) override {
    last_actions_ = actions;
    pending_act_callback_ = std::move(callback);
  }

  const std::optional<std::string>& last_start_session_id() const {
    return last_start_session_id_;
  }
  const std::optional<optimization_guide::proto::BrowserStartTask>&
  last_start_request() const {
    return last_start_request_;
  }
  StartTaskCallback TakeStartCallback() {
    return std::move(pending_start_callback_);
  }

  const std::optional<TaskId>& last_stop_task_id() const {
    return last_stop_task_id_;
  }
  const std::vector<TaskId>& stopped_task_ids() const {
    return stopped_task_ids_;
  }
  StopTaskCallback TakeStopCallback() {
    return std::move(pending_stop_callback_);
  }

  const std::optional<optimization_guide::proto::Actions>& last_actions()
      const {
    return last_actions_;
  }
  ActCallback TakeActCallback() { return std::move(pending_act_callback_); }

 private:
  std::optional<std::string> last_start_session_id_;
  std::optional<optimization_guide::proto::BrowserStartTask>
      last_start_request_;
  StartTaskCallback pending_start_callback_;

  std::optional<TaskId> last_stop_task_id_;
  std::vector<TaskId> stopped_task_ids_;
  StopTaskCallback pending_stop_callback_;

  std::optional<optimization_guide::proto::Actions> last_actions_;
  ActCallback pending_act_callback_;
};

class ActuationTransportHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    auto delegate = std::make_unique<FakeActuationDelegate>();
    delegate_ = delegate.get();
    handler_ = std::make_unique<ActuationTransportHandler>(&session_,
                                                           std::move(delegate));
  }

  void TearDown() override {
    delegate_ = nullptr;
    handler_.reset();
  }

  void StartSessionTask(int32_t task_id) {
    proto::ActuationDownstreamMessage start_msg;
    start_msg.set_request_id("req_start");
    start_msg.mutable_start_task();
    handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                        start_msg.SerializeAsString());
    optimization_guide::proto::BrowserStartTaskResult result;
    result.set_task_id(task_id);
    result.set_status(
        optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
    delegate_->TakeStartCallback().Run(result);
  }

  FakeTransportSession session_{"session_123"};
  raw_ptr<FakeActuationDelegate> delegate_ = nullptr;
  std::unique_ptr<ActuationTransportHandler> handler_;
};

TEST_F(ActuationTransportHandlerTest,
       StartTaskForwardsAndSendsUpstreamResponse) {
  proto::ActuationDownstreamMessage downstream;
  downstream.set_request_id("req_start_1");
  downstream.mutable_start_task()->set_tab_id(42);

  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      downstream.SerializeAsString());

  ASSERT_TRUE(delegate_->last_start_session_id().has_value());
  EXPECT_EQ(*delegate_->last_start_session_id(), "session_123");
  ASSERT_TRUE(delegate_->last_start_request().has_value());
  EXPECT_EQ(delegate_->last_start_request()->tab_id(), 42);

  optimization_guide::proto::BrowserStartTaskResult result;
  result.set_task_id(7);
  result.set_tab_id(42);
  result.set_status(optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
  delegate_->TakeStartCallback().Run(result);

  ASSERT_EQ(session_.sent_messages().size(), 1u);
  EXPECT_EQ(session_.sent_payload_types()[0],
            browser_actuator::PayloadType::kActuation);
  const auto& upstream = session_.sent_messages()[0];
  EXPECT_EQ(upstream.request_id(), "req_start_1");
  ASSERT_TRUE(upstream.has_start_task_result());
  EXPECT_EQ(upstream.start_task_result().task_id(), 7);
  EXPECT_EQ(upstream.start_task_result().tab_id(), 42);
  EXPECT_EQ(upstream.start_task_result().status(),
            optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
}

TEST_F(ActuationTransportHandlerTest,
       StopTaskForwardsOwnedTaskAndSendsResponse) {
  StartSessionTask(7);

  proto::ActuationDownstreamMessage downstream;
  downstream.set_request_id("req_stop_1");
  downstream.mutable_stop_task()->set_task_id(7);

  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      downstream.SerializeAsString());

  ASSERT_TRUE(delegate_->last_stop_task_id().has_value());
  EXPECT_EQ(*delegate_->last_stop_task_id(), TaskId(7));

  delegate_->TakeStopCallback().Run(true);

  ASSERT_EQ(session_.sent_messages().size(), 2u);
  const auto& upstream = session_.sent_messages()[1];
  EXPECT_EQ(upstream.request_id(), "req_stop_1");
  ASSERT_TRUE(upstream.has_stop_task_result());
  EXPECT_EQ(upstream.stop_task_result().task_id(), 7);
  EXPECT_EQ(upstream.stop_task_result().status(),
            proto::StopTaskResult::STOP_TASK_STATUS_SUCCESS);

  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      downstream.SerializeAsString());
  ASSERT_EQ(session_.sent_messages().size(), 3u);
  EXPECT_EQ(session_.sent_messages()[2].stop_task_result().task_id(), 7);
  EXPECT_EQ(session_.sent_messages()[2].stop_task_result().status(),
            proto::StopTaskResult::STOP_TASK_STATUS_FAILURE);
}

TEST_F(ActuationTransportHandlerTest, StopTaskRejectsUnownedTask) {
  proto::ActuationDownstreamMessage downstream;
  downstream.set_request_id("req_stop_unowned");
  downstream.mutable_stop_task()->set_task_id(99);

  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      downstream.SerializeAsString());

  EXPECT_FALSE(delegate_->last_stop_task_id().has_value());
  ASSERT_EQ(session_.sent_messages().size(), 1u);
  const auto& upstream = session_.sent_messages()[0];
  EXPECT_EQ(upstream.request_id(), "req_stop_unowned");
  ASSERT_TRUE(upstream.has_stop_task_result());
  EXPECT_EQ(upstream.stop_task_result().task_id(), 99);
  EXPECT_EQ(upstream.stop_task_result().status(),
            proto::StopTaskResult::STOP_TASK_STATUS_FAILURE);
}

TEST_F(ActuationTransportHandlerTest, ActForwardsOwnedTaskAndSendsResponse) {
  StartSessionTask(7);

  proto::ActuationDownstreamMessage downstream;
  downstream.set_request_id("req_act_1");
  downstream.mutable_actions()->set_task_id(7);

  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      downstream.SerializeAsString());

  ASSERT_TRUE(delegate_->last_actions().has_value());
  EXPECT_EQ(delegate_->last_actions()->task_id(), 7);

  optimization_guide::proto::ActionsResult actions_result;
  actions_result.set_action_result(0);
  delegate_->TakeActCallback().Run(actions_result);

  ASSERT_EQ(session_.sent_messages().size(), 2u);
  const auto& upstream = session_.sent_messages()[1];
  EXPECT_EQ(upstream.request_id(), "req_act_1");
  ASSERT_TRUE(upstream.has_actions_result());
  EXPECT_EQ(upstream.actions_result().action_result(), 0);
}

TEST_F(ActuationTransportHandlerTest, ActRejectsUnownedTask) {
  proto::ActuationDownstreamMessage downstream;
  downstream.set_request_id("req_act_unowned");
  downstream.mutable_actions()->set_task_id(99);

  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      downstream.SerializeAsString());

  EXPECT_FALSE(delegate_->last_actions().has_value());
  ASSERT_EQ(session_.sent_messages().size(), 1u);
  const auto& upstream = session_.sent_messages()[0];
  EXPECT_EQ(upstream.request_id(), "req_act_unowned");
  ASSERT_TRUE(upstream.has_actions_result());
  EXPECT_EQ(upstream.actions_result().action_result(),
            static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
}

TEST_F(ActuationTransportHandlerTest,
       IgnoresInvalidPayloadOrNonActuationPayloadType) {
  handler_->OnMessage(browser_actuator::PayloadType::kControl, "invalid_bytes");
  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      "\xFF\xFF\xFF\xFF");

  proto::ActuationDownstreamMessage empty_command;
  empty_command.set_request_id("req_empty");
  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      empty_command.SerializeAsString());

  EXPECT_FALSE(delegate_->last_start_request().has_value());
  EXPECT_FALSE(delegate_->last_stop_task_id().has_value());
  EXPECT_FALSE(delegate_->last_actions().has_value());
  EXPECT_TRUE(session_.sent_messages().empty());
}

TEST_F(ActuationTransportHandlerTest, StopsActiveSessionTasksOnDestruction) {
  FakeTransportSession session("session_teardown");
  std::vector<TaskId> stopped_tasks;

  class DestructionTrackingDelegate : public FakeActuationDelegate {
   public:
    explicit DestructionTrackingDelegate(std::vector<TaskId>* stopped_tasks)
        : stopped_tasks_(stopped_tasks) {}
    void StopTask(TaskId task_id, StopTaskCallback callback) override {
      stopped_tasks_->push_back(task_id);
      std::move(callback).Run(true);
    }

   private:
    raw_ptr<std::vector<TaskId>> stopped_tasks_;
  };

  auto delegate = std::make_unique<DestructionTrackingDelegate>(&stopped_tasks);
  DestructionTrackingDelegate* raw_delegate = delegate.get();
  auto handler = std::make_unique<ActuationTransportHandler>(
      &session, std::move(delegate));

  proto::ActuationDownstreamMessage start_msg;
  start_msg.set_request_id("req_start_1");
  start_msg.mutable_start_task();
  handler->OnMessage(browser_actuator::PayloadType::kActuation,
                     start_msg.SerializeAsString());
  optimization_guide::proto::BrowserStartTaskResult result1;
  result1.set_task_id(11);
  result1.set_status(
      optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
  raw_delegate->TakeStartCallback().Run(result1);

  start_msg.set_request_id("req_start_2");
  handler->OnMessage(browser_actuator::PayloadType::kActuation,
                     start_msg.SerializeAsString());
  optimization_guide::proto::BrowserStartTaskResult result2;
  result2.set_task_id(22);
  result2.set_status(
      optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
  raw_delegate->TakeStartCallback().Run(result2);

  proto::ActuationDownstreamMessage stop_msg;
  stop_msg.set_request_id("req_stop_11");
  stop_msg.mutable_stop_task()->set_task_id(11);
  handler->OnMessage(browser_actuator::PayloadType::kActuation,
                     stop_msg.SerializeAsString());
  EXPECT_EQ(stopped_tasks, std::vector<TaskId>{TaskId(11)});

  stopped_tasks.clear();
  handler.reset();
  EXPECT_EQ(stopped_tasks, std::vector<TaskId>{TaskId(22)});
}

}  // namespace
}  // namespace actor

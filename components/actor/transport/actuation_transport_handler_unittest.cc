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
#include "base/strings/strcat.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/actor/transport/actuation_delegate.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/payload_type_mapping.h"
#include "components/stream_transport/public/transport_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

class FakeTransportSession
    : public browser_actuator::transport::TransportSession {
 public:
  explicit FakeTransportSession(std::string_view session_id)
      : session_id_(session_id) {}
  ~FakeTransportSession() override = default;

  std::string_view GetSessionId() const override { return session_id_; }

  base::expected<void, browser_actuator::transport::SendUpstreamMessageError>
  SendUpstreamMessage(browser_actuator::transport::PayloadType payload_type,
                      const google::protobuf::MessageLite& message) override {
    sent_payload_types_.push_back(payload_type);
    proto::BrowserToServerPayload upstream;
    EXPECT_TRUE(upstream.ParseFromString(message.SerializeAsString()));
    sent_messages_.push_back(std::move(upstream));
    return {};
  }

  void OnMessage(browser_actuator::transport::PayloadType payload_type,
                 std::string_view serialized_payload) override {}

  void Close() override {}

  const std::vector<browser_actuator::transport::PayloadType>&
  sent_payload_types() const {
    return sent_payload_types_;
  }

  const std::vector<proto::BrowserToServerPayload>& sent_messages() const {
    return sent_messages_;
  }

 private:
  std::string session_id_;
  std::vector<browser_actuator::transport::PayloadType> sent_payload_types_;
  std::vector<proto::BrowserToServerPayload> sent_messages_;
};

class FakeActuationDelegate : public ActuationDelegate {
 public:
  FakeActuationDelegate() = default;
  ~FakeActuationDelegate() override = default;

  void StartTask(const proto::StartTaskRequest& request,
                 StartTaskCallback callback) override {
    ++start_task_call_count_;
    last_start_request_ = request;
    pending_start_callback_ = std::move(callback);
  }

  void StopTask(TaskId task_id,
                const proto::StopTaskRequest& request,
                StopTaskCallback callback) override {
    last_stop_task_id_ = task_id;
    last_stop_request_ = request;
    stopped_task_ids_.push_back(task_id);
    std::move(callback).Run(true);
  }

  void Act(TaskId task_id,
           const proto::PerformActionsRequest& request,
           ActCallback callback) override {
    last_act_task_id_ = task_id;
    last_perform_actions_request_ = request;
    pending_act_callback_ = std::move(callback);
  }

  int start_task_call_count() const { return start_task_call_count_; }
  const std::optional<proto::StartTaskRequest>& last_start_request() const {
    return last_start_request_;
  }
  StartTaskCallback TakeStartCallback() {
    return std::move(pending_start_callback_);
  }

  const std::optional<TaskId>& last_stop_task_id() const {
    return last_stop_task_id_;
  }
  const std::optional<proto::StopTaskRequest>& last_stop_request() const {
    return last_stop_request_;
  }
  const std::vector<TaskId>& stopped_task_ids() const {
    return stopped_task_ids_;
  }

  const std::optional<TaskId>& last_act_task_id() const {
    return last_act_task_id_;
  }
  const std::optional<proto::PerformActionsRequest>&
  last_perform_actions_request() const {
    return last_perform_actions_request_;
  }
  ActCallback TakeActCallback() { return std::move(pending_act_callback_); }

 private:
  int start_task_call_count_ = 0;
  std::optional<proto::StartTaskRequest> last_start_request_;
  StartTaskCallback pending_start_callback_;

  std::optional<TaskId> last_stop_task_id_;
  std::optional<proto::StopTaskRequest> last_stop_request_;
  std::vector<TaskId> stopped_task_ids_;

  std::optional<TaskId> last_act_task_id_;
  std::optional<proto::PerformActionsRequest> last_perform_actions_request_;
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

  void StartSessionTask(int32_t task_id, std::string_view external_task_id) {
    proto::ServerToBrowserPayload start_msg;
    start_msg.set_task_id(std::string(external_task_id));
    start_msg.mutable_start_task();
    handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                        start_msg.SerializeAsString());
    proto::StartTaskResponse response;
    response.set_status(proto::StartTaskResponse::STATUS_OK);
    delegate_->TakeStartCallback().Run(TaskId(task_id), std::move(response));
  }

  FakeTransportSession session_{"session_123"};
  raw_ptr<FakeActuationDelegate> delegate_ = nullptr;
  std::unique_ptr<ActuationTransportHandler> handler_;
};

TEST_F(ActuationTransportHandlerTest,
       StartTaskForwardsAndSendsUpstreamResponse) {
  proto::ServerToBrowserPayload downstream;
  downstream.set_task_id("server-task-1");
  downstream.set_request_id("req_start_1");
  downstream.mutable_start_task()->set_task_title("Book flight");
  downstream.mutable_start_task()->add_target_surfaces()->set_tab_id(42);

  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());

  ASSERT_TRUE(delegate_->last_start_request().has_value());
  EXPECT_EQ(delegate_->last_start_request()->task_title(), "Book flight");
  ASSERT_EQ(delegate_->last_start_request()->target_surfaces_size(), 1);
  EXPECT_EQ(delegate_->last_start_request()->target_surfaces(0).tab_id(), 42);

  proto::StartTaskResponse response;
  response.set_status(proto::StartTaskResponse::STATUS_OK);
  response.add_bound_surfaces()->set_tab_id(42);
  delegate_->TakeStartCallback().Run(TaskId(7), std::move(response));

  ASSERT_EQ(session_.sent_messages().size(), 1u);
  EXPECT_EQ(session_.sent_payload_types()[0],
            browser_actuator::transport::PayloadType::kActuation);
  const auto& upstream = session_.sent_messages()[0];
  EXPECT_EQ(upstream.task_id(), "server-task-1");
  EXPECT_EQ(upstream.request_id(), "req_start_1");
  ASSERT_TRUE(upstream.has_start_task());
  EXPECT_EQ(upstream.start_task().status(),
            proto::StartTaskResponse::STATUS_OK);
  ASSERT_EQ(upstream.start_task().bound_surfaces_size(), 1);
  EXPECT_EQ(upstream.start_task().bound_surfaces(0).tab_id(), 42);
}

TEST_F(ActuationTransportHandlerTest, StartTaskWithoutTaskIdIsRejected) {
  proto::ServerToBrowserPayload start_msg;
  start_msg.set_request_id("req_start_no_id");
  start_msg.mutable_start_task()->set_task_title("Book flight");

  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      start_msg.SerializeAsString());

  EXPECT_EQ(delegate_->start_task_call_count(), 0);
  ASSERT_EQ(session_.sent_messages().size(), 1u);
  const auto& reply = session_.sent_messages()[0];
  EXPECT_TRUE(reply.task_id().empty());
  EXPECT_EQ(reply.request_id(), "req_start_no_id");
  ASSERT_TRUE(reply.has_start_task());
  EXPECT_EQ(reply.start_task().status(),
            proto::StartTaskResponse::STATUS_MISSING_TASK_ID);
}

TEST_F(ActuationTransportHandlerTest,
       StartTaskWithServerTaskIdRoutesPerformActionsAndStopTask) {
  proto::ServerToBrowserPayload start_msg;
  start_msg.set_task_id("server-task-1");
  start_msg.set_request_id("req_start_1");
  start_msg.mutable_start_task()->set_task_title("Book flight");

  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      start_msg.SerializeAsString());

  ASSERT_TRUE(delegate_->last_start_request().has_value());
  EXPECT_EQ(delegate_->last_start_request()->task_title(), "Book flight");

  proto::StartTaskResponse start_response;
  start_response.set_status(proto::StartTaskResponse::STATUS_OK);
  delegate_->TakeStartCallback().Run(TaskId(7), std::move(start_response));

  ASSERT_EQ(session_.sent_messages().size(), 1u);
  EXPECT_EQ(session_.sent_messages()[0].task_id(), "server-task-1");
  EXPECT_EQ(session_.sent_messages()[0].request_id(), "req_start_1");
  EXPECT_EQ(session_.sent_messages()[0].start_task().status(),
            proto::StartTaskResponse::STATUS_OK);

  proto::ServerToBrowserPayload act_msg;
  act_msg.set_task_id("server-task-1");
  act_msg.set_request_id("req_act_1");
  act_msg.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      act_msg.SerializeAsString());

  ASSERT_TRUE(delegate_->last_act_task_id().has_value());
  EXPECT_EQ(*delegate_->last_act_task_id(), TaskId(7));

  proto::PerformActionsResponse act_response;
  act_response.set_status(proto::PerformActionsResponse::STATUS_OK);
  delegate_->TakeActCallback().Run(std::move(act_response));

  ASSERT_EQ(session_.sent_messages().size(), 2u);
  EXPECT_EQ(session_.sent_messages()[1].task_id(), "server-task-1");
  EXPECT_EQ(session_.sent_messages()[1].request_id(), "req_act_1");
  EXPECT_EQ(session_.sent_messages()[1].perform_actions().status(),
            proto::PerformActionsResponse::STATUS_OK);

  proto::ServerToBrowserPayload stop_msg;
  stop_msg.set_task_id("server-task-1");
  stop_msg.set_request_id("req_stop_1");
  stop_msg.mutable_stop_task()->set_reason(
      proto::StopTaskRequest::TASK_COMPLETED);
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_msg.SerializeAsString());

  ASSERT_TRUE(delegate_->last_stop_task_id().has_value());
  EXPECT_EQ(*delegate_->last_stop_task_id(), TaskId(7));
  ASSERT_EQ(session_.sent_messages().size(), 3u);
  EXPECT_EQ(session_.sent_messages()[2].task_id(), "server-task-1");
  EXPECT_EQ(session_.sent_messages()[2].request_id(), "req_stop_1");
  EXPECT_EQ(session_.sent_messages()[2].stop_task().status(),
            proto::StopTaskResponse::STATUS_OK);
}

TEST_F(ActuationTransportHandlerTest,
       DuplicateStartTaskIdCreatesNoSecondChromeTask) {
  StartSessionTask(7, "server-task-1");
  EXPECT_EQ(delegate_->start_task_call_count(), 1);
  ASSERT_EQ(session_.sent_messages().size(), 1u);

  proto::ServerToBrowserPayload duplicate_start;
  duplicate_start.set_task_id("server-task-1");
  duplicate_start.set_request_id("req_start_dup");
  duplicate_start.mutable_start_task()->set_task_title("Duplicate task");
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      duplicate_start.SerializeAsString());

  EXPECT_EQ(delegate_->start_task_call_count(), 1);
  ASSERT_EQ(session_.sent_messages().size(), 2u);
  const auto& dup_reply = session_.sent_messages()[1];
  EXPECT_EQ(dup_reply.task_id(), "server-task-1");
  EXPECT_EQ(dup_reply.request_id(), "req_start_dup");
  ASSERT_TRUE(dup_reply.has_start_task());
  EXPECT_EQ(dup_reply.start_task().status(),
            proto::StartTaskResponse::STATUS_DUPLICATE_TASK_ID);

  // Original task mapping remains intact.
  proto::ServerToBrowserPayload act_msg;
  act_msg.set_task_id("server-task-1");
  act_msg.set_request_id("req_act_after_dup");
  act_msg.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      act_msg.SerializeAsString());
  ASSERT_TRUE(delegate_->last_act_task_id().has_value());
  EXPECT_EQ(*delegate_->last_act_task_id(), TaskId(7));
}

TEST_F(ActuationTransportHandlerTest,
       DuplicateStartTaskIdWhileStartIsPendingIsRejected) {
  proto::ServerToBrowserPayload first_start;
  first_start.set_task_id("server-task-1");
  first_start.set_request_id("req_start_1");
  first_start.mutable_start_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      first_start.SerializeAsString());
  EXPECT_EQ(delegate_->start_task_call_count(), 1);
  // The delegate has not replied yet, so nothing has been sent upstream.
  EXPECT_TRUE(session_.sent_messages().empty());

  proto::ServerToBrowserPayload duplicate_start;
  duplicate_start.set_task_id("server-task-1");
  duplicate_start.set_request_id("req_start_dup");
  duplicate_start.mutable_start_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      duplicate_start.SerializeAsString());

  EXPECT_EQ(delegate_->start_task_call_count(), 1);
  ASSERT_EQ(session_.sent_messages().size(), 1u);
  EXPECT_EQ(session_.sent_messages()[0].request_id(), "req_start_dup");
  EXPECT_EQ(session_.sent_messages()[0].start_task().status(),
            proto::StartTaskResponse::STATUS_DUPLICATE_TASK_ID);

  // Completing the original start still records the mapping.
  proto::StartTaskResponse response;
  response.set_status(proto::StartTaskResponse::STATUS_OK);
  delegate_->TakeStartCallback().Run(TaskId(7), std::move(response));
  ASSERT_EQ(session_.sent_messages().size(), 2u);
  EXPECT_EQ(session_.sent_messages()[1].request_id(), "req_start_1");
  EXPECT_EQ(session_.sent_messages()[1].start_task().status(),
            proto::StartTaskResponse::STATUS_OK);

  proto::ServerToBrowserPayload act_msg;
  act_msg.set_task_id("server-task-1");
  act_msg.set_request_id("req_act_1");
  act_msg.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      act_msg.SerializeAsString());
  ASSERT_TRUE(delegate_->last_act_task_id().has_value());
  EXPECT_EQ(*delegate_->last_act_task_id(), TaskId(7));
}

TEST_F(ActuationTransportHandlerTest, TaskIdCanBeReusedAfterFailedStartTask) {
  proto::ServerToBrowserPayload start_msg;
  start_msg.set_task_id("server-task-1");
  start_msg.set_request_id("req_start_1");
  start_msg.mutable_start_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      start_msg.SerializeAsString());
  proto::StartTaskResponse failed_response;
  failed_response.set_status(
      proto::StartTaskResponse::STATUS_TARGET_TAB_NOT_FOUND);
  delegate_->TakeStartCallback().Run(TaskId(), std::move(failed_response));
  ASSERT_EQ(session_.sent_messages().size(), 1u);
  EXPECT_EQ(session_.sent_messages()[0].start_task().status(),
            proto::StartTaskResponse::STATUS_TARGET_TAB_NOT_FOUND);

  // The failed start released the ID, so a retry reaches the delegate.
  StartSessionTask(7, "server-task-1");
  EXPECT_EQ(delegate_->start_task_call_count(), 2);
  ASSERT_EQ(session_.sent_messages().size(), 2u);
  EXPECT_EQ(session_.sent_messages()[1].start_task().status(),
            proto::StartTaskResponse::STATUS_OK);
}

TEST_F(ActuationTransportHandlerTest,
       TwoServerTasksInOneSessionRouteToTheirOwnTaskIds) {
  StartSessionTask(10, "server-task-a");
  StartSessionTask(20, "server-task-b");

  proto::ServerToBrowserPayload act_b;
  act_b.set_task_id("server-task-b");
  act_b.set_request_id("req_act_b");
  act_b.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      act_b.SerializeAsString());
  ASSERT_TRUE(delegate_->last_act_task_id().has_value());
  EXPECT_EQ(*delegate_->last_act_task_id(), TaskId(20));
  proto::PerformActionsResponse response_b;
  response_b.set_status(proto::PerformActionsResponse::STATUS_OK);
  delegate_->TakeActCallback().Run(std::move(response_b));
  EXPECT_EQ(session_.sent_messages().back().task_id(), "server-task-b");

  proto::ServerToBrowserPayload act_a;
  act_a.set_task_id("server-task-a");
  act_a.set_request_id("req_act_a");
  act_a.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      act_a.SerializeAsString());
  ASSERT_TRUE(delegate_->last_act_task_id().has_value());
  EXPECT_EQ(*delegate_->last_act_task_id(), TaskId(10));
  proto::PerformActionsResponse response_a;
  response_a.set_status(proto::PerformActionsResponse::STATUS_OK);
  delegate_->TakeActCallback().Run(std::move(response_a));
  EXPECT_EQ(session_.sent_messages().back().task_id(), "server-task-a");

  proto::ServerToBrowserPayload stop_a;
  stop_a.set_task_id("server-task-a");
  stop_a.set_request_id("req_stop_a");
  stop_a.mutable_stop_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_a.SerializeAsString());
  ASSERT_TRUE(delegate_->last_stop_task_id().has_value());
  EXPECT_EQ(*delegate_->last_stop_task_id(), TaskId(10));
}

TEST_F(ActuationTransportHandlerTest,
       StopTaskRemovesMappingSoSubsequentPerformActionsFails) {
  StartSessionTask(7, "server-task-1");

  proto::ServerToBrowserPayload stop_msg;
  stop_msg.set_task_id("server-task-1");
  stop_msg.set_request_id("req_stop_1");
  stop_msg.mutable_stop_task()->set_reason(
      proto::StopTaskRequest::TASK_COMPLETED);
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_msg.SerializeAsString());
  ASSERT_EQ(session_.sent_messages().size(), 2u);
  EXPECT_EQ(session_.sent_messages()[1].stop_task().status(),
            proto::StopTaskResponse::STATUS_OK);

  proto::ServerToBrowserPayload act_msg;
  act_msg.set_task_id("server-task-1");
  act_msg.set_request_id("req_act_after_stop");
  act_msg.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      act_msg.SerializeAsString());

  EXPECT_FALSE(delegate_->last_act_task_id().has_value());
  ASSERT_EQ(session_.sent_messages().size(), 3u);
  const auto& act_reply = session_.sent_messages()[2];
  EXPECT_EQ(act_reply.task_id(), "server-task-1");
  EXPECT_EQ(act_reply.request_id(), "req_act_after_stop");
  ASSERT_TRUE(act_reply.has_perform_actions());
  EXPECT_EQ(act_reply.perform_actions().status(),
            proto::PerformActionsResponse::STATUS_TAB_DISCONNECTED);
  EXPECT_EQ(act_reply.perform_actions().action_status_code(),
            static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
}

TEST_F(ActuationTransportHandlerTest,
       StopTaskForwardsOwnedTaskOnceAndSendsResponse) {
  StartSessionTask(7, "server-task-7");

  proto::ServerToBrowserPayload downstream;
  downstream.set_task_id("server-task-7");
  downstream.set_request_id("req_stop_1");
  downstream.mutable_stop_task()->set_reason(
      proto::StopTaskRequest::USER_CANCELLED);

  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());

  ASSERT_TRUE(delegate_->last_stop_task_id().has_value());
  EXPECT_EQ(*delegate_->last_stop_task_id(), TaskId(7));
  ASSERT_TRUE(delegate_->last_stop_request().has_value());
  EXPECT_EQ(delegate_->last_stop_request()->reason(),
            proto::StopTaskRequest::USER_CANCELLED);
  EXPECT_EQ(delegate_->stopped_task_ids().size(), 1u);
  ASSERT_EQ(session_.sent_messages().size(), 2u);
  EXPECT_EQ(session_.sent_messages()[1].task_id(), "server-task-7");
  EXPECT_EQ(session_.sent_messages()[1].request_id(), "req_stop_1");
  ASSERT_TRUE(session_.sent_messages()[1].has_stop_task());
  EXPECT_EQ(session_.sent_messages()[1].stop_task().status(),
            proto::StopTaskResponse::STATUS_OK);

  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());
  EXPECT_EQ(delegate_->stopped_task_ids().size(), 1u);
  ASSERT_EQ(session_.sent_messages().size(), 3u);
  EXPECT_EQ(session_.sent_messages()[2].task_id(), "server-task-7");
  EXPECT_EQ(session_.sent_messages()[2].request_id(), "req_stop_1");
  ASSERT_TRUE(session_.sent_messages()[2].has_stop_task());
  EXPECT_EQ(session_.sent_messages()[2].stop_task().status(),
            proto::StopTaskResponse::STATUS_FAILED);
}

TEST_F(ActuationTransportHandlerTest, StopTaskRejectsUnownedTask) {
  proto::ServerToBrowserPayload downstream;
  downstream.set_task_id("server-task-unowned");
  downstream.set_request_id("req_stop_unowned");
  downstream.mutable_stop_task()->set_reason(
      proto::StopTaskRequest::TASK_COMPLETED);

  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());
  EXPECT_FALSE(delegate_->last_stop_task_id().has_value());
  ASSERT_EQ(session_.sent_messages().size(), 1u);
  EXPECT_EQ(session_.sent_messages()[0].task_id(), "server-task-unowned");
  EXPECT_EQ(session_.sent_messages()[0].request_id(), "req_stop_unowned");
  ASSERT_TRUE(session_.sent_messages()[0].has_stop_task());
  EXPECT_EQ(session_.sent_messages()[0].stop_task().status(),
            proto::StopTaskResponse::STATUS_FAILED);

  downstream.set_task_id("unknown-task");
  downstream.set_request_id("req_stop_unknown");
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());
  EXPECT_FALSE(delegate_->last_stop_task_id().has_value());
  ASSERT_EQ(session_.sent_messages().size(), 2u);
  EXPECT_EQ(session_.sent_messages()[1].task_id(), "unknown-task");
  EXPECT_EQ(session_.sent_messages()[1].request_id(), "req_stop_unknown");
  ASSERT_TRUE(session_.sent_messages()[1].has_stop_task());
  EXPECT_EQ(session_.sent_messages()[1].stop_task().status(),
            proto::StopTaskResponse::STATUS_FAILED);
}

TEST_F(ActuationTransportHandlerTest,
       StopTaskWhileStartIsPendingStopsTaskOnceStarted) {
  proto::ServerToBrowserPayload start_msg;
  start_msg.set_task_id("server-task-1");
  start_msg.set_request_id("req_start_1");
  start_msg.mutable_start_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      start_msg.SerializeAsString());

  proto::ServerToBrowserPayload stop_msg;
  stop_msg.set_task_id("server-task-1");
  stop_msg.set_request_id("req_stop_1");
  stop_msg.mutable_stop_task()->set_reason(
      proto::StopTaskRequest::USER_CANCELLED);
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_msg.SerializeAsString());
  // The stop is queued until the start completes.
  EXPECT_FALSE(delegate_->last_stop_task_id().has_value());
  EXPECT_TRUE(session_.sent_messages().empty());

  proto::StartTaskResponse response;
  response.set_status(proto::StartTaskResponse::STATUS_OK);
  delegate_->TakeStartCallback().Run(TaskId(7), std::move(response));

  ASSERT_TRUE(delegate_->last_stop_task_id().has_value());
  EXPECT_EQ(*delegate_->last_stop_task_id(), TaskId(7));
  ASSERT_TRUE(delegate_->last_stop_request().has_value());
  EXPECT_EQ(delegate_->last_stop_request()->reason(),
            proto::StopTaskRequest::USER_CANCELLED);
  ASSERT_EQ(session_.sent_messages().size(), 2u);
  EXPECT_EQ(session_.sent_messages()[0].request_id(), "req_start_1");
  EXPECT_EQ(session_.sent_messages()[0].start_task().status(),
            proto::StartTaskResponse::STATUS_OK);
  EXPECT_EQ(session_.sent_messages()[1].task_id(), "server-task-1");
  EXPECT_EQ(session_.sent_messages()[1].request_id(), "req_stop_1");
  ASSERT_TRUE(session_.sent_messages()[1].has_stop_task());
  EXPECT_EQ(session_.sent_messages()[1].stop_task().status(),
            proto::StopTaskResponse::STATUS_OK);

  // The stopped task is not tracked, so later actions fail.
  proto::ServerToBrowserPayload act_msg;
  act_msg.set_task_id("server-task-1");
  act_msg.set_request_id("req_act_after_stop");
  act_msg.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      act_msg.SerializeAsString());
  EXPECT_FALSE(delegate_->last_act_task_id().has_value());
  ASSERT_EQ(session_.sent_messages().size(), 3u);
  EXPECT_EQ(session_.sent_messages()[2].perform_actions().status(),
            proto::PerformActionsResponse::STATUS_TAB_DISCONNECTED);
}

TEST_F(ActuationTransportHandlerTest,
       StopTaskWhileStartIsPendingFailsIfStartFails) {
  proto::ServerToBrowserPayload start_msg;
  start_msg.set_task_id("server-task-1");
  start_msg.set_request_id("req_start_1");
  start_msg.mutable_start_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      start_msg.SerializeAsString());

  proto::ServerToBrowserPayload stop_msg;
  stop_msg.set_task_id("server-task-1");
  stop_msg.set_request_id("req_stop_1");
  stop_msg.mutable_stop_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_msg.SerializeAsString());
  EXPECT_TRUE(session_.sent_messages().empty());

  proto::StartTaskResponse failed_response;
  failed_response.set_status(
      proto::StartTaskResponse::STATUS_TARGET_TAB_NOT_FOUND);
  delegate_->TakeStartCallback().Run(TaskId(), std::move(failed_response));

  EXPECT_FALSE(delegate_->last_stop_task_id().has_value());
  ASSERT_EQ(session_.sent_messages().size(), 2u);
  EXPECT_EQ(session_.sent_messages()[0].request_id(), "req_start_1");
  EXPECT_EQ(session_.sent_messages()[0].start_task().status(),
            proto::StartTaskResponse::STATUS_TARGET_TAB_NOT_FOUND);
  EXPECT_EQ(session_.sent_messages()[1].task_id(), "server-task-1");
  EXPECT_EQ(session_.sent_messages()[1].request_id(), "req_stop_1");
  ASSERT_TRUE(session_.sent_messages()[1].has_stop_task());
  EXPECT_EQ(session_.sent_messages()[1].stop_task().status(),
            proto::StopTaskResponse::STATUS_FAILED);
}

TEST_F(ActuationTransportHandlerTest,
       SecondStopTaskWhileStartIsPendingIsRejected) {
  proto::ServerToBrowserPayload start_msg;
  start_msg.set_task_id("server-task-1");
  start_msg.set_request_id("req_start_1");
  start_msg.mutable_start_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      start_msg.SerializeAsString());

  proto::ServerToBrowserPayload stop_msg;
  stop_msg.set_task_id("server-task-1");
  stop_msg.set_request_id("req_stop_1");
  stop_msg.mutable_stop_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_msg.SerializeAsString());
  EXPECT_TRUE(session_.sent_messages().empty());

  stop_msg.set_request_id("req_stop_2");
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_msg.SerializeAsString());
  ASSERT_EQ(session_.sent_messages().size(), 1u);
  EXPECT_EQ(session_.sent_messages()[0].request_id(), "req_stop_2");
  EXPECT_EQ(session_.sent_messages()[0].stop_task().status(),
            proto::StopTaskResponse::STATUS_FAILED);

  // The first stop still runs once the start completes.
  proto::StartTaskResponse response;
  response.set_status(proto::StartTaskResponse::STATUS_OK);
  delegate_->TakeStartCallback().Run(TaskId(7), std::move(response));
  EXPECT_EQ(delegate_->stopped_task_ids(), std::vector<TaskId>{TaskId(7)});
  ASSERT_EQ(session_.sent_messages().size(), 3u);
  EXPECT_EQ(session_.sent_messages()[2].request_id(), "req_stop_1");
  EXPECT_EQ(session_.sent_messages()[2].stop_task().status(),
            proto::StopTaskResponse::STATUS_OK);
}

TEST_F(ActuationTransportHandlerTest, ActWhileStartIsPendingIsRejected) {
  proto::ServerToBrowserPayload start_msg;
  start_msg.set_task_id("server-task-1");
  start_msg.set_request_id("req_start_1");
  start_msg.mutable_start_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      start_msg.SerializeAsString());

  proto::ServerToBrowserPayload act_msg;
  act_msg.set_task_id("server-task-1");
  act_msg.set_request_id("req_act_1");
  act_msg.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      act_msg.SerializeAsString());

  EXPECT_FALSE(delegate_->last_act_task_id().has_value());
  ASSERT_EQ(session_.sent_messages().size(), 1u);
  const auto& act_reply = session_.sent_messages()[0];
  EXPECT_EQ(act_reply.task_id(), "server-task-1");
  EXPECT_EQ(act_reply.request_id(), "req_act_1");
  ASSERT_TRUE(act_reply.has_perform_actions());
  EXPECT_EQ(act_reply.perform_actions().status(),
            proto::PerformActionsResponse::STATUS_ACTION_FAILED);
  EXPECT_EQ(
      act_reply.perform_actions().action_status_code(),
      static_cast<int32_t>(mojom::ActionResultCode::kInvalidTaskStateForAct));

  // Once the start completes, actions reach the delegate.
  proto::StartTaskResponse response;
  response.set_status(proto::StartTaskResponse::STATUS_OK);
  delegate_->TakeStartCallback().Run(TaskId(7), std::move(response));
  act_msg.set_request_id("req_act_2");
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      act_msg.SerializeAsString());
  ASSERT_TRUE(delegate_->last_act_task_id().has_value());
  EXPECT_EQ(*delegate_->last_act_task_id(), TaskId(7));
}

TEST_F(ActuationTransportHandlerTest, ActForwardsOwnedTaskAndSendsResponse) {
  StartSessionTask(7, "server-task-7");

  proto::ServerToBrowserPayload downstream;
  downstream.set_task_id("server-task-7");
  downstream.set_request_id("req_act_1");
  downstream.mutable_perform_actions()->add_actions()->mutable_wait();

  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());

  ASSERT_TRUE(delegate_->last_act_task_id().has_value());
  EXPECT_EQ(*delegate_->last_act_task_id(), TaskId(7));
  ASSERT_TRUE(delegate_->last_perform_actions_request().has_value());
  EXPECT_EQ(delegate_->last_perform_actions_request()->actions_size(), 1);

  proto::PerformActionsResponse act_response;
  act_response.set_status(proto::PerformActionsResponse::STATUS_OK);
  act_response.set_action_status_code(0);
  delegate_->TakeActCallback().Run(std::move(act_response));

  ASSERT_EQ(session_.sent_messages().size(), 2u);
  const auto& upstream = session_.sent_messages()[1];
  EXPECT_EQ(upstream.task_id(), "server-task-7");
  EXPECT_EQ(upstream.request_id(), "req_act_1");
  ASSERT_TRUE(upstream.has_perform_actions());
  EXPECT_EQ(upstream.perform_actions().status(),
            proto::PerformActionsResponse::STATUS_OK);
  EXPECT_EQ(upstream.perform_actions().action_status_code(), 0);
}

TEST_F(ActuationTransportHandlerTest,
       ActErasesActiveTaskWhenTaskStoppedDuringAct) {
  StartSessionTask(7, "7");
  StartSessionTask(8, "8");

  proto::ServerToBrowserPayload downstream;
  downstream.set_task_id("7");
  downstream.set_request_id("req_act_task_went_away");
  downstream.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());

  proto::PerformActionsResponse task_went_away_response;
  task_went_away_response.set_status(
      proto::PerformActionsResponse::STATUS_TAB_DISCONNECTED);
  task_went_away_response.set_action_status_code(
      static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
  delegate_->TakeActCallback().Run(std::move(task_went_away_response));

  // Subsequent `StopTask` for task 7 should see it already removed from
  // `active_tasks_` and not invoke `delegate_->StopTask(TaskId(7))`.
  proto::ServerToBrowserPayload stop_msg;
  stop_msg.set_task_id("7");
  stop_msg.set_request_id("req_stop_after_task_went_away");
  stop_msg.mutable_stop_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_msg.SerializeAsString());
  EXPECT_TRUE(delegate_->stopped_task_ids().empty());

  downstream.set_task_id("8");
  downstream.set_request_id("req_act_user_navigated_away");
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());

  proto::PerformActionsResponse user_navigated_away_response;
  user_navigated_away_response.set_status(
      proto::PerformActionsResponse::STATUS_ACTION_FAILED);
  user_navigated_away_response.set_action_status_code(
      static_cast<int32_t>(mojom::ActionResultCode::kUserNavigatedAway));
  delegate_->TakeActCallback().Run(std::move(user_navigated_away_response));

  stop_msg.set_task_id("8");
  stop_msg.set_request_id("req_stop_after_user_navigated_away");
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_msg.SerializeAsString());
  EXPECT_TRUE(delegate_->stopped_task_ids().empty());
}

TEST_F(ActuationTransportHandlerTest,
       ActRetainsActiveTaskOnNonTerminalActionFailure) {
  StartSessionTask(7, "7");

  proto::ServerToBrowserPayload downstream;
  downstream.set_task_id("7");
  downstream.set_request_id("req_act_recoverable_fail");
  downstream.mutable_perform_actions()->add_actions()->mutable_wait();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());

  proto::PerformActionsResponse action_failed_response;
  action_failed_response.set_status(
      proto::PerformActionsResponse::STATUS_ACTION_FAILED);
  action_failed_response.set_action_status_code(
      static_cast<int32_t>(mojom::ActionResultCode::kElementDisabled));
  delegate_->TakeActCallback().Run(std::move(action_failed_response));

  // A recoverable failure (or `kTabWentAway` on a single action) keeps task 7
  // in `active_tasks_`, allowing subsequent `PerformActions` and `StopTask`
  // requests to reach `delegate_`.
  downstream.set_request_id("req_act_tab_went_away");
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());
  ASSERT_TRUE(delegate_->last_act_task_id().has_value());
  EXPECT_EQ(*delegate_->last_act_task_id(), TaskId(7));

  proto::PerformActionsResponse tab_went_away_response;
  tab_went_away_response.set_status(
      proto::PerformActionsResponse::STATUS_TAB_DISCONNECTED);
  tab_went_away_response.set_action_status_code(
      static_cast<int32_t>(mojom::ActionResultCode::kTabWentAway));
  delegate_->TakeActCallback().Run(std::move(tab_went_away_response));

  proto::ServerToBrowserPayload stop_msg;
  stop_msg.set_task_id("7");
  stop_msg.set_request_id("req_stop_after_recoverable_fail");
  stop_msg.mutable_stop_task();
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      stop_msg.SerializeAsString());
  EXPECT_EQ(delegate_->stopped_task_ids(), std::vector<TaskId>{TaskId(7)});
}

TEST_F(ActuationTransportHandlerTest, ActRejectsUnownedTask) {
  proto::ServerToBrowserPayload downstream;
  downstream.set_task_id("server-task-unowned");
  downstream.set_request_id("req_act_unowned");
  downstream.mutable_perform_actions();

  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      downstream.SerializeAsString());

  EXPECT_FALSE(delegate_->last_act_task_id().has_value());
  ASSERT_EQ(session_.sent_messages().size(), 1u);
  const auto& upstream = session_.sent_messages()[0];
  EXPECT_EQ(upstream.task_id(), "server-task-unowned");
  EXPECT_EQ(upstream.request_id(), "req_act_unowned");
  ASSERT_TRUE(upstream.has_perform_actions());
  EXPECT_EQ(upstream.perform_actions().status(),
            proto::PerformActionsResponse::STATUS_TAB_DISCONNECTED);
  EXPECT_EQ(upstream.perform_actions().action_status_code(),
            static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
}

TEST_F(ActuationTransportHandlerTest,
       IgnoresInvalidPayloadOrNonActuationPayloadType) {
  handler_->OnMessage(browser_actuator::transport::PayloadType::kControl,
                      "invalid_bytes");
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      "\xFF\xFF\xFF\xFF");

  proto::ServerToBrowserPayload empty_command;
  empty_command.set_task_id("7");
  handler_->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                      empty_command.SerializeAsString());

  EXPECT_FALSE(delegate_->last_start_request().has_value());
  EXPECT_FALSE(delegate_->last_stop_task_id().has_value());
  EXPECT_FALSE(delegate_->last_act_task_id().has_value());
  EXPECT_TRUE(session_.sent_messages().empty());
}

TEST_F(ActuationTransportHandlerTest, StopsActiveSessionTasksOnDestruction) {
  FakeTransportSession session("session_teardown");
  std::vector<TaskId> stopped_tasks;

  class DestructionTrackingDelegate : public FakeActuationDelegate {
   public:
    explicit DestructionTrackingDelegate(std::vector<TaskId>* stopped_tasks)
        : stopped_tasks_(stopped_tasks) {}
    void StopTask(TaskId task_id,
                  const proto::StopTaskRequest& request,
                  StopTaskCallback callback) override {
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

  auto send_start_task = [&](std::string_view external_task_id) {
    proto::ServerToBrowserPayload start_msg;
    start_msg.set_task_id(std::string(external_task_id));
    start_msg.mutable_start_task();
    handler->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                       start_msg.SerializeAsString());
  };

  send_start_task("server-task-11");
  proto::StartTaskResponse response1;
  response1.set_status(proto::StartTaskResponse::STATUS_OK);
  raw_delegate->TakeStartCallback().Run(TaskId(11), std::move(response1));

  send_start_task("server-task-22");
  proto::StartTaskResponse response2;
  response2.set_status(proto::StartTaskResponse::STATUS_OK);
  raw_delegate->TakeStartCallback().Run(TaskId(22), std::move(response2));

  send_start_task("server-task-33");
  proto::StartTaskResponse response3;
  response3.set_status(proto::StartTaskResponse::STATUS_OK);
  raw_delegate->TakeStartCallback().Run(TaskId(33), std::move(response3));

  send_start_task("server-task-44");
  proto::StartTaskResponse failed_response;
  failed_response.set_status(
      proto::StartTaskResponse::STATUS_TARGET_TAB_NOT_FOUND);
  raw_delegate->TakeStartCallback().Run(TaskId(), std::move(failed_response));
  EXPECT_EQ(session.sent_messages().back().task_id(), "server-task-44");

  proto::ServerToBrowserPayload stop_msg;
  stop_msg.set_task_id("server-task-11");
  stop_msg.mutable_stop_task();
  handler->OnMessage(browser_actuator::transport::PayloadType::kActuation,
                     stop_msg.SerializeAsString());
  EXPECT_EQ(stopped_tasks, std::vector<TaskId>{TaskId(11)});

  stopped_tasks.clear();
  handler.reset();
  EXPECT_EQ(stopped_tasks, (std::vector<TaskId>{TaskId(22), TaskId(33)}));
}

TEST_F(ActuationTransportHandlerTest,
       ExpectedTypeUrlMatchesServerToBrowserPayload) {
  EXPECT_EQ(browser_actuator::transport::ExpectedTypeUrl(
                browser_actuator::transport::PayloadType::kActuation),
            base::StrCat({browser_actuator::transport::kTypeUrlPrefix,
                          proto::ServerToBrowserPayload().GetTypeName()}));
}

}  // namespace
}  // namespace actor

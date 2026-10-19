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
#include "components/actor/transport/actuation_delegate.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/payload_type_mapping.h"
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
    proto::BrowserToServerPayload upstream;
    EXPECT_TRUE(upstream.ParseFromString(message.SerializeAsString()));
    sent_messages_.push_back(std::move(upstream));
    return {};
  }

  void OnMessage(browser_actuator::PayloadType payload_type,
                 const google::protobuf::MessageLite& message) override {}

  const std::vector<browser_actuator::PayloadType>& sent_payload_types() const {
    return sent_payload_types_;
  }

  const std::vector<proto::BrowserToServerPayload>& sent_messages() const {
    return sent_messages_;
  }

 private:
  std::string session_id_;
  std::vector<browser_actuator::PayloadType> sent_payload_types_;
  std::vector<proto::BrowserToServerPayload> sent_messages_;
};

class FakeActuationDelegate : public ActuationDelegate {
 public:
  FakeActuationDelegate() = default;
  ~FakeActuationDelegate() override = default;

  void StartTask(const proto::StartTaskRequest& request,
                 StartTaskCallback callback) override {
    last_start_request_ = request;
    pending_start_callback_ = std::move(callback);
  }

  void StopTask(TaskId task_id, StopTaskCallback callback) override {
    stopped_task_ids_.push_back(task_id);
    std::move(callback).Run(true);
  }

  void Act(const optimization_guide::proto::Actions& actions,
           ActCallback callback) override {}

  const std::optional<proto::StartTaskRequest>& last_start_request() const {
    return last_start_request_;
  }
  StartTaskCallback TakeStartCallback() {
    return std::move(pending_start_callback_);
  }

  const std::vector<TaskId>& stopped_task_ids() const {
    return stopped_task_ids_;
  }

 private:
  std::optional<proto::StartTaskRequest> last_start_request_;
  StartTaskCallback pending_start_callback_;

  std::vector<TaskId> stopped_task_ids_;
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

  FakeTransportSession session_{"session_123"};
  raw_ptr<FakeActuationDelegate> delegate_ = nullptr;
  std::unique_ptr<ActuationTransportHandler> handler_;
};

TEST_F(ActuationTransportHandlerTest,
       StartTaskForwardsAndSendsUpstreamResponse) {
  proto::ServerToBrowserPayload downstream;
  downstream.set_request_id("req_start_1");
  downstream.mutable_start_task()->set_task_title("Book flight");
  downstream.mutable_start_task()->add_target_surfaces()->set_tab_id(42);

  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
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
            browser_actuator::PayloadType::kActuation);
  const auto& upstream = session_.sent_messages()[0];
  EXPECT_EQ(upstream.task_id(), "7");
  EXPECT_EQ(upstream.request_id(), "req_start_1");
  ASSERT_TRUE(upstream.has_start_task());
  EXPECT_EQ(upstream.start_task().status(),
            proto::StartTaskResponse::STATUS_OK);
  ASSERT_EQ(upstream.start_task().bound_surfaces_size(), 1);
  EXPECT_EQ(upstream.start_task().bound_surfaces(0).tab_id(), 42);
}

TEST_F(ActuationTransportHandlerTest,
       IgnoresInvalidPayloadOrNonActuationPayloadType) {
  handler_->OnMessage(browser_actuator::PayloadType::kControl, "invalid_bytes");
  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      "\xFF\xFF\xFF\xFF");

  proto::ServerToBrowserPayload empty_command;
  empty_command.set_task_id("7");
  handler_->OnMessage(browser_actuator::PayloadType::kActuation,
                      empty_command.SerializeAsString());

  EXPECT_FALSE(delegate_->last_start_request().has_value());
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

  proto::ServerToBrowserPayload start_msg;
  start_msg.mutable_start_task();
  handler->OnMessage(browser_actuator::PayloadType::kActuation,
                     start_msg.SerializeAsString());
  proto::StartTaskResponse response1;
  response1.set_status(proto::StartTaskResponse::STATUS_OK);
  raw_delegate->TakeStartCallback().Run(TaskId(11), std::move(response1));

  handler->OnMessage(browser_actuator::PayloadType::kActuation,
                     start_msg.SerializeAsString());
  proto::StartTaskResponse response2;
  response2.set_status(proto::StartTaskResponse::STATUS_OK);
  raw_delegate->TakeStartCallback().Run(TaskId(22), std::move(response2));

  handler->OnMessage(browser_actuator::PayloadType::kActuation,
                     start_msg.SerializeAsString());
  proto::StartTaskResponse failed_response;
  failed_response.set_status(
      proto::StartTaskResponse::STATUS_TARGET_TAB_NOT_FOUND);
  raw_delegate->TakeStartCallback().Run(TaskId(), std::move(failed_response));
  EXPECT_TRUE(session.sent_messages().back().task_id().empty());

  handler.reset();
  EXPECT_EQ(stopped_tasks, (std::vector<TaskId>{TaskId(11), TaskId(22)}));
}

TEST_F(ActuationTransportHandlerTest,
       ExpectedTypeUrlMatchesServerToBrowserPayload) {
  EXPECT_EQ(browser_actuator::ExpectedTypeUrl(
                browser_actuator::PayloadType::kActuation),
            base::StrCat({browser_actuator::kTypeUrlPrefix,
                          proto::ServerToBrowserPayload().GetTypeName()}));
}

}  // namespace
}  // namespace actor

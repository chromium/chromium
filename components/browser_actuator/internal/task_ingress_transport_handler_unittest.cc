// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/task_ingress_transport_handler.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/payload_type_mapping.h"
#include "components/stream_transport/public/transport_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

using ::testing::_;
using ::testing::StrictMock;

class FakeTransportSession : public stream_transport::TransportSession {
 public:
  FakeTransportSession() = default;
  ~FakeTransportSession() override = default;

  // stream_transport::TransportSession:
  std::string_view GetSessionId() const override { return "test-session"; }

  base::expected<void, stream_transport::SendUpstreamMessageError>
  SendUpstreamMessage(stream_transport::PayloadType payload_type,
                      const google::protobuf::MessageLite& message) override {
    return base::ok();
  }

  void OnMessage(stream_transport::PayloadType payload_type,
                 std::string_view serialized_payload) override {}

  void Close() override { closed_ = true; }

  bool closed() const { return closed_; }

 private:
  bool closed_ = false;
};

class MockTaskIngressEventDelegate : public TaskIngressEventDelegate {
 public:
  // TaskIngressEventDelegate:
  MOCK_METHOD(void, OnTaskStartedEvent, (), (override));
  MOCK_METHOD(void, OnWorklogUpdateEvent, (std::string_view), (override));
  MOCK_METHOD(void,
              OnTaskCompletedEvent,
              (TaskCompletionStatus, std::string_view),
              (override));
  MOCK_METHOD(void, DetachForShutdown, (), (override));

  base::WeakPtr<MockTaskIngressEventDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockTaskIngressEventDelegate> weak_ptr_factory_{this};
};

class TaskIngressTransportHandlerTest : public ::testing::Test {
 protected:
  FakeTransportSession session_;
  StrictMock<MockTaskIngressEventDelegate> delegate_;
};

TEST_F(TaskIngressTransportHandlerTest, ExpectedTypeUrlMatchesProtoTypeName) {
  TaskIngressDownstreamPayload payload;
  std::string expected =
      base::StrCat({stream_transport::kTypeUrlPrefix, payload.GetTypeName()});
  EXPECT_EQ(stream_transport::ExpectedTypeUrl(
                stream_transport::PayloadType::kTaskIngress),
            expected);
}

TEST_F(TaskIngressTransportHandlerTest, IgnoresNonTaskIngressPayloadType) {
  TaskIngressTransportHandler handler(&session_, delegate_.GetWeakPtr());

  TaskIngressDownstreamPayload payload;
  payload.mutable_task_started();
  std::string serialized = payload.SerializeAsString();

  handler.OnMessage(stream_transport::PayloadType::kControl, serialized);
  EXPECT_FALSE(session_.closed());
}

TEST_F(TaskIngressTransportHandlerTest, IgnoresMalformedPayload) {
  TaskIngressTransportHandler handler(&session_, delegate_.GetWeakPtr());

  handler.OnMessage(stream_transport::PayloadType::kTaskIngress,
                    "not-a-valid-proto");
  EXPECT_FALSE(session_.closed());
}

TEST_F(TaskIngressTransportHandlerTest, IgnoresEventNotSet) {
  TaskIngressTransportHandler handler(&session_, delegate_.GetWeakPtr());

  TaskIngressDownstreamPayload payload;
  std::string serialized = payload.SerializeAsString();

  handler.OnMessage(stream_transport::PayloadType::kTaskIngress, serialized);
  EXPECT_FALSE(session_.closed());
}

TEST_F(TaskIngressTransportHandlerTest, DispatchesTaskStartedEvent) {
  TaskIngressTransportHandler handler(&session_, delegate_.GetWeakPtr());

  TaskIngressDownstreamPayload payload;
  payload.mutable_task_started();
  std::string serialized = payload.SerializeAsString();

  EXPECT_CALL(delegate_, OnTaskStartedEvent());
  handler.OnMessage(stream_transport::PayloadType::kTaskIngress, serialized);
  EXPECT_FALSE(session_.closed());
}

TEST_F(TaskIngressTransportHandlerTest, DispatchesWorklogUpdateEvent) {
  TaskIngressTransportHandler handler(&session_, delegate_.GetWeakPtr());

  TaskIngressDownstreamPayload payload;
  payload.mutable_worklog_update()->set_step_description("Navigating to page");
  std::string serialized = payload.SerializeAsString();

  EXPECT_CALL(delegate_, OnWorklogUpdateEvent("Navigating to page"));
  handler.OnMessage(stream_transport::PayloadType::kTaskIngress, serialized);
  EXPECT_FALSE(session_.closed());
}

TEST_F(TaskIngressTransportHandlerTest,
       DispatchesTaskCompletedEventWithoutClosingSession) {
  TaskIngressTransportHandler handler(&session_, delegate_.GetWeakPtr());

  TaskIngressDownstreamPayload payload;
  TaskCompletedEvent* completed = payload.mutable_task_completed();
  completed->set_status(TASK_COMPLETION_STATUS_SUCCEEDED);
  completed->set_error_message("Diagnostic detail");
  std::string serialized = payload.SerializeAsString();

  EXPECT_CALL(delegate_,
              OnTaskCompletedEvent(TASK_COMPLETION_STATUS_SUCCEEDED,
                                   std::string_view("Diagnostic detail")));
  handler.OnMessage(stream_transport::PayloadType::kTaskIngress, serialized);

  // Session must NOT be closed by the handler per Design Doc Section 5.2.
  EXPECT_FALSE(session_.closed());
}

TEST_F(TaskIngressTransportHandlerTest, ToleratesExpiredDelegateWeakPtr) {
  auto local_delegate =
      std::make_unique<StrictMock<MockTaskIngressEventDelegate>>();
  TaskIngressTransportHandler handler(&session_, local_delegate->GetWeakPtr());
  local_delegate.reset();

  TaskIngressDownstreamPayload payload;
  payload.mutable_task_started();
  std::string serialized = payload.SerializeAsString();

  // No crash or call when delegate is expired.
  handler.OnMessage(stream_transport::PayloadType::kTaskIngress, serialized);
  EXPECT_FALSE(session_.closed());
}

}  // namespace
}  // namespace browser_actuator

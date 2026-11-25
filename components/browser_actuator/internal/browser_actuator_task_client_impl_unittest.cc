// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_task_client_impl.h"

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/browser_actuator/internal/task_ingress_transport_handler_factory.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/browser_actuator/public/proto/auto_password_change.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_handler.h"
#include "components/stream_transport/public/transport_service.h"
#include "components/stream_transport/public/transport_session.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace browser_actuator {
namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::StrictMock;

AutoPasswordChangeTaskPayload MakeSamplePayload() {
  AutoPasswordChangeTaskPayload payload;
  payload.set_change_password_url(
      "https://example.com/.well-known/change-password");
  payload.set_domain("example.com");
  return payload;
}

class FakeTransportSession : public stream_transport::TransportSession {
 public:
  FakeTransportSession(std::string_view session_id, base::OnceClosure on_close)
      : session_id_(session_id), on_close_(std::move(on_close)) {}
  ~FakeTransportSession() override = default;

  void SetHandler(std::unique_ptr<stream_transport::TransportHandler> handler) {
    handler_ = std::move(handler);
  }

  // stream_transport::TransportSession:
  std::string_view GetSessionId() const override { return session_id_; }

  base::expected<void, stream_transport::SendUpstreamMessageError>
  SendUpstreamMessage(stream_transport::PayloadType payload_type,
                      const google::protobuf::MessageLite& message) override {
    return base::ok();
  }

  void OnMessage(stream_transport::PayloadType payload_type,
                 std::string_view serialized_payload) override {
    if (handler_) {
      handler_->OnMessage(payload_type, serialized_payload);
    }
  }

  void Close() override {
    if (base::OnceClosure cb = std::move(on_close_)) {
      std::move(cb).Run();
    }
  }

 private:
  std::string session_id_;
  base::OnceClosure on_close_;
  // Declared last so `~FakeTransportSession()` destroys `handler_` (which holds
  // a `raw_ptr<TransportSession>` to `this`) before `this` is destroyed.
  std::unique_ptr<stream_transport::TransportHandler> handler_;
};

// Models `ClientSessionStarter` and `TransportSessionRegistryImpl` ownership:
// `Close()` synchronously frees the underlying `FakeTransportSession` so
// PartitionAlloc's `DanglingPtrDetector` catches any dangling `raw_ptr`.
class FakeTransportService : public stream_transport::TransportService {
 public:
  FakeTransportService() = default;
  ~FakeTransportService() override = default;

  // stream_transport::TransportService:
  stream_transport::TransportChannel* GetChannel() override { return nullptr; }
  stream_transport::TransportSession* CreateSession(
      std::string_view session_id,
      stream_transport::InitIngressSource ingress_source) override {
    return nullptr;
  }
  stream_transport::TransportSession* GetSession(
      std::string_view session_id) override {
    for (const auto& session : active_sessions_) {
      if (session->GetSessionId() == session_id) {
        return session.get();
      }
    }
    return nullptr;
  }
  stream_transport::SessionStreamRecorderFactory* GetStreamRecorderFactory()
      override {
    return nullptr;
  }
  stream_transport::TransportSession* StartClientSession(
      std::string_view session_id,
      stream_transport::PayloadType payload_type,
      const google::protobuf::MessageLite& message,
      StartClientSessionCallback callback) override {
    ++start_call_count_;
    last_session_id_ = std::string(session_id);
    last_payload_type_ = payload_type;
    last_serialized_message_ = message.SerializeAsString();
    start_callback_ = std::move(callback);
    pending_cancelled_ = false;
    std::string pending_id = base::StrCat({"pending-", session_id});
    pending_session_ = std::make_unique<FakeTransportSession>(
        pending_id, base::BindOnce(&FakeTransportService::OnPendingClosed,
                                   base::Unretained(this)));
    return pending_session_.get();
  }

  void RespondSuccess(std::string_view session_id) {
    std::string id(session_id);
    auto matured = std::make_unique<FakeTransportSession>(
        id, base::BindOnce(&FakeTransportService::OnActiveClosed,
                           base::Unretained(this), id));
    FakeTransportSession* raw = matured.get();
    active_sessions_.push_back(std::move(matured));
    // Mirror `ClientSessionStarter`: keep `pending_session_` alive until
    // `start_callback_` finishes, then free it immediately.
    auto pending_lifetime = std::move(pending_session_);
    if (start_callback_) {
      std::move(start_callback_).Run(raw);
    }
  }

  void RespondFailure(stream_transport::StartClientSessionError error) {
    auto pending_lifetime = std::move(pending_session_);
    if (start_callback_) {
      std::move(start_callback_).Run(base::unexpected(error));
    }
  }

  void ClearActiveSessionsExternally() { active_sessions_.clear(); }

  int start_call_count() const { return start_call_count_; }
  const std::string& last_session_id() const { return last_session_id_; }
  stream_transport::PayloadType last_payload_type() const {
    return last_payload_type_;
  }
  const std::string& last_serialized_message() const {
    return last_serialized_message_;
  }
  bool pending_cancelled() const { return pending_cancelled_; }
  bool WasSessionClosed(std::string_view session_id) const {
    return closed_session_ids_.contains(std::string(session_id));
  }

 private:
  void OnPendingClosed() {
    pending_cancelled_ = true;
    pending_session_.reset();
  }

  void OnActiveClosed(const std::string& session_id) {
    closed_session_ids_.insert(session_id);
    std::erase_if(active_sessions_, [&session_id](const auto& session) {
      return session->GetSessionId() == session_id;
    });
  }

  int start_call_count_ = 0;
  std::string last_session_id_;
  stream_transport::PayloadType last_payload_type_ =
      stream_transport::PayloadType::kControl;
  std::string last_serialized_message_;
  StartClientSessionCallback start_callback_;
  std::unique_ptr<FakeTransportSession> pending_session_;
  std::vector<std::unique_ptr<FakeTransportSession>> active_sessions_;
  bool pending_cancelled_ = false;
  std::set<std::string> closed_session_ids_;
};

class MockTaskClientObserver : public BrowserActuatorTaskClient::Observer {
 public:
  MockTaskClientObserver() = default;
  ~MockTaskClientObserver() override = default;

  // BrowserActuatorTaskClient::Observer:
  MOCK_METHOD(void, OnTaskStarted, (), (override));
  MOCK_METHOD(void, OnWorklogUpdate, (std::string_view), (override));
  MOCK_METHOD(void,
              OnTaskCompleted,
              (const BrowserActuatorTaskClient::Result&),
              (override));
};

class SelfDeletingObserver : public BrowserActuatorTaskClient::Observer {
 public:
  explicit SelfDeletingObserver(
      std::unique_ptr<BrowserActuatorTaskClientImpl>* client_holder)
      : client_holder_(CHECK_DEREF(client_holder)) {}

  // BrowserActuatorTaskClient::Observer:
  void OnTaskCompleted(
      const BrowserActuatorTaskClient::Result& result) override {
    called_ = true;
    client_holder_->reset();
  }

  bool called() const { return called_; }

 private:
  const raw_ref<std::unique_ptr<BrowserActuatorTaskClientImpl>> client_holder_;
  bool called_ = false;
};

class BrowserActuatorTaskClientImplTest : public ::testing::Test {
 protected:
  BrowserActuatorTaskClientImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    handler_factory_ = std::make_unique<TaskIngressTransportHandlerFactory>();
    client_ = std::make_unique<BrowserActuatorTaskClientImpl>(
        &transport_service_, handler_factory_->GetWeakPtr(),
        base::BindRepeating([]() { return std::string("test-client-guid"); }));
  }

  base::test::TaskEnvironment task_environment_;
  FakeTransportService transport_service_;
  std::unique_ptr<TaskIngressTransportHandlerFactory> handler_factory_;
  StrictMock<MockTaskClientObserver> observer_;
  std::unique_ptr<BrowserActuatorTaskClientImpl> client_;
};

TEST_F(BrowserActuatorTaskClientImplTest, StartTaskSendsRequestAndAccepts) {
  AutoPasswordChangeTaskPayload payload = MakeSamplePayload();
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     payload, &observer_);

  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kStarting);
  EXPECT_FALSE(client_->session_id().empty());
  EXPECT_EQ(transport_service_.last_session_id(), client_->session_id());
  EXPECT_EQ(transport_service_.last_payload_type(),
            stream_transport::PayloadType::kTaskIngress);

  TaskIngressUpstreamPayload upstream_proto;
  ASSERT_TRUE(upstream_proto.ParseFromString(
      transport_service_.last_serialized_message()));
  ASSERT_TRUE(upstream_proto.has_start_task());
  EXPECT_EQ(upstream_proto.start_task().use_case(),
            TASK_USE_CASE_AUTO_PASSWORD_CHANGE);
  EXPECT_EQ(upstream_proto.start_task().client_id(), "test-client-guid");
  EXPECT_EQ(
      upstream_proto.start_task().feature_payload().type_url(),
      "type.googleapis.com/browser_actuator.AutoPasswordChangeTaskPayload");

  AutoPasswordChangeTaskPayload unpacked;
  ASSERT_TRUE(unpacked.ParseFromString(
      upstream_proto.start_task().feature_payload().value()));
  EXPECT_EQ(unpacked.change_password_url(), payload.change_password_url());
  EXPECT_EQ(unpacked.domain(), payload.domain());

  transport_service_.RespondSuccess(client_->session_id());
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kAccepted);
}

TEST_F(BrowserActuatorTaskClientImplTest,
       StartTaskPreservesPrePackedCustomPrefixAnyPayload) {
  StartTaskCommand holder;
  holder.mutable_feature_payload()->set_type_url(
      "type.corp.google.com/internal.CustomTaskPayload");
  holder.mutable_feature_payload()->set_value("serialized-custom-bytes");

  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     holder.feature_payload(), &observer_);

  TaskIngressUpstreamPayload upstream_proto;
  ASSERT_TRUE(upstream_proto.ParseFromString(
      transport_service_.last_serialized_message()));
  EXPECT_EQ(upstream_proto.start_task().feature_payload().type_url(),
            "type.corp.google.com/internal.CustomTaskPayload");
  EXPECT_EQ(upstream_proto.start_task().feature_payload().value(),
            "serialized-custom-bytes");
}

TEST_F(BrowserActuatorTaskClientImplTest,
       StartTaskWithEmptyDeviceIdFailsWithoutStartingSession) {
  auto client_without_guid = std::make_unique<BrowserActuatorTaskClientImpl>(
      &transport_service_, handler_factory_->GetWeakPtr(),
      base::BindRepeating([]() { return std::string(); }));

  client_without_guid->StartTask(
      BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
      MakeSamplePayload(), &observer_);
  EXPECT_EQ(client_without_guid->state(),
            BrowserActuatorTaskClientImpl::State::kDone);
  EXPECT_EQ(transport_service_.start_call_count(), 0);

  EXPECT_CALL(observer_,
              OnTaskCompleted(AllOf(
                  Field(&BrowserActuatorTaskClient::Result::status,
                        BrowserActuatorTaskClient::CompletionStatus::kFailed),
                  Field(&BrowserActuatorTaskClient::Result::error_message,
                        Not(IsEmpty())))));
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(BrowserActuatorTaskClientImplTest, StartSessionFailureCompletesTask) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);

  transport_service_.RespondFailure(
      stream_transport::StartClientSessionError::kChannelUnavailable);
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);

  // `OnTaskCompleted()` must fire asynchronously on a subsequent task turn.
  EXPECT_CALL(observer_,
              OnTaskCompleted(AllOf(
                  Field(&BrowserActuatorTaskClient::Result::status,
                        BrowserActuatorTaskClient::CompletionStatus::kFailed),
                  Field(&BrowserActuatorTaskClient::Result::error_message,
                        Not(IsEmpty())))));
  task_environment_.FastForwardBy(base::TimeDelta());

  // Subsequent `Cancel()` in `State::kDone` is a no-op.
  client_->Cancel();
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(BrowserActuatorTaskClientImplTest,
       CancelWhileStartingAbortsPendingSessionAndNotifies) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kStarting);
  EXPECT_FALSE(transport_service_.pending_cancelled());

  client_->Cancel();
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);
  EXPECT_TRUE(transport_service_.pending_cancelled());

  EXPECT_CALL(observer_,
              OnTaskCompleted(Field(
                  &BrowserActuatorTaskClient::Result::status,
                  BrowserActuatorTaskClient::CompletionStatus::kCancelled)));
  // Subsequent session response closes any late-created session immediately.
  transport_service_.RespondSuccess(session_id);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(transport_service_.WasSessionClosed(session_id));
}

TEST_F(BrowserActuatorTaskClientImplTest,
       CancelTransitionsToDoneAndNotifiesObserver) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  transport_service_.RespondSuccess(session_id);
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kAccepted);

  client_->Cancel();
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);
  EXPECT_TRUE(transport_service_.WasSessionClosed(session_id));

  EXPECT_CALL(observer_,
              OnTaskCompleted(Field(
                  &BrowserActuatorTaskClient::Result::status,
                  BrowserActuatorTaskClient::CompletionStatus::kCancelled)));
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(BrowserActuatorTaskClientImplTest,
       ExternalSessionTeardownBeforeCancelAndDestructionIsSafe) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  transport_service_.RespondSuccess(session_id);
  ASSERT_NE(nullptr, transport_service_.GetSession(session_id));

  // Simulate `TransportSessionRegistryImpl::Clear()` freeing active sessions.
  transport_service_.ClearActiveSessionsExternally();
  EXPECT_EQ(nullptr, transport_service_.GetSession(session_id));

  client_->Cancel();
  EXPECT_CALL(observer_,
              OnTaskCompleted(Field(
                  &BrowserActuatorTaskClient::Result::status,
                  BrowserActuatorTaskClient::CompletionStatus::kCancelled)));
  task_environment_.FastForwardBy(base::TimeDelta());
  client_.reset();
}

TEST_F(BrowserActuatorTaskClientImplTest, CancelWhenIdleIsNoOp) {
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kIdle);
  client_->Cancel();
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kIdle);

  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kStarting);
}

TEST_F(BrowserActuatorTaskClientImplTest,
       DestructorWhileStartingAbortsPendingSessionAndIgnoresLateResponse) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  EXPECT_FALSE(transport_service_.pending_cancelled());

  EXPECT_CALL(observer_, OnTaskCompleted).Times(0);
  client_.reset();
  EXPECT_TRUE(transport_service_.pending_cancelled());

  transport_service_.RespondSuccess(session_id);
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(BrowserActuatorTaskClientImplTest,
       DestructorClosesActiveSessionWithoutObserver) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  transport_service_.RespondSuccess(session_id);
  EXPECT_FALSE(transport_service_.WasSessionClosed(session_id));

  client_.reset();
  EXPECT_TRUE(transport_service_.WasSessionClosed(session_id));
}

TEST_F(BrowserActuatorTaskClientImplTest,
       DestructionBeforePostedCompletionDropsCallback) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);

  EXPECT_CALL(observer_, OnTaskCompleted).Times(0);
  client_->Cancel();
  client_.reset();

  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(BrowserActuatorTaskClientImplTest,
       SelfDeletingObserverInAsyncCompletionIsSafe) {
  SelfDeletingObserver deleting_observer(&client_);
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &deleting_observer);
  client_->Cancel();
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(deleting_observer.called());
  EXPECT_EQ(nullptr, client_);
}

TEST_F(BrowserActuatorTaskClientImplTest,
       DownstreamEventsRouteThroughHandlerAndIgnorePreStartWorklog) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  transport_service_.RespondSuccess(session_id);
  auto* session = static_cast<FakeTransportSession*>(
      transport_service_.GetSession(session_id));
  ASSERT_NE(nullptr, session);

  std::unique_ptr<stream_transport::TransportHandler> handler =
      handler_factory_->OnNewSession(session);
  ASSERT_NE(nullptr, handler);
  session->SetHandler(std::move(handler));

  // Worklog updates prior to `TaskStartedEvent` are ignored.
  TaskIngressDownstreamPayload early_worklog;
  early_worklog.mutable_worklog_update()->set_step_description("Early step");
  session->OnMessage(stream_transport::PayloadType::kTaskIngress,
                     early_worklog.SerializeAsString());
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kAccepted);

  EXPECT_CALL(observer_, OnTaskStarted()).Times(1);
  TaskIngressDownstreamPayload started;
  started.mutable_task_started();
  session->OnMessage(stream_transport::PayloadType::kTaskIngress,
                     started.SerializeAsString());
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kRunning);

  // Duplicate `TaskStartedEvent` in `kRunning` is ignored.
  session->OnMessage(stream_transport::PayloadType::kTaskIngress,
                     started.SerializeAsString());

  EXPECT_CALL(observer_, OnWorklogUpdate(std::string_view("Step 1")));
  TaskIngressDownstreamPayload worklog;
  worklog.mutable_worklog_update()->set_step_description("Step 1");
  session->OnMessage(stream_transport::PayloadType::kTaskIngress,
                     worklog.SerializeAsString());

  EXPECT_CALL(
      observer_,
      OnTaskCompleted(
          AllOf(Field(&BrowserActuatorTaskClient::Result::status,
                      BrowserActuatorTaskClient::CompletionStatus::kSucceeded),
                Field(&BrowserActuatorTaskClient::Result::error_message, ""))));
  TaskIngressDownstreamPayload completed;
  completed.mutable_task_completed()->set_status(
      TASK_COMPLETION_STATUS_SUCCEEDED);
  session->OnMessage(stream_transport::PayloadType::kTaskIngress,
                     completed.SerializeAsString());

  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);
  EXPECT_TRUE(transport_service_.WasSessionClosed(session_id));
  FakeTransportSession probe_session(session_id, base::OnceClosure());
  EXPECT_EQ(nullptr, handler_factory_->OnNewSession(&probe_session));
}

TEST_F(BrowserActuatorTaskClientImplTest,
       DirectAcceptedToCompletedForwardsErrorAndStatusMappings) {
  for (const auto& [proto_status, expected_status] :
       std::vector<std::pair<TaskCompletionStatus,
                             BrowserActuatorTaskClient::CompletionStatus>>{
           {TASK_COMPLETION_STATUS_FAILED,
            BrowserActuatorTaskClient::CompletionStatus::kFailed},
           {TASK_COMPLETION_STATUS_CANCELLED,
            BrowserActuatorTaskClient::CompletionStatus::kCancelled},
           {TASK_COMPLETION_STATUS_UNSPECIFIED,
            BrowserActuatorTaskClient::CompletionStatus::kFailed},
       }) {
    auto local_client = std::make_unique<BrowserActuatorTaskClientImpl>(
        &transport_service_, handler_factory_->GetWeakPtr(),
        base::BindRepeating([]() { return std::string("test-client-guid"); }));
    local_client->StartTask(
        BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
        MakeSamplePayload(), &observer_);
    const std::string sid = local_client->session_id();
    transport_service_.RespondSuccess(sid);

    EXPECT_CALL(
        observer_,
        OnTaskCompleted(AllOf(
            Field(&BrowserActuatorTaskClient::Result::status, expected_status),
            Field(&BrowserActuatorTaskClient::Result::error_message,
                  "diagnostic-reason"))));
    local_client->OnTaskCompletedEvent(proto_status, "diagnostic-reason");
    EXPECT_EQ(local_client->state(),
              BrowserActuatorTaskClientImpl::State::kDone);
  }
}

TEST_F(BrowserActuatorTaskClientImplTest,
       ReentrancyThroughHandlerOnMessageDestroyingClientAndHandlerIsSafe) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  transport_service_.RespondSuccess(session_id);

  auto* session = static_cast<FakeTransportSession*>(
      transport_service_.GetSession(session_id));
  ASSERT_NE(nullptr, session);
  std::unique_ptr<stream_transport::TransportHandler> handler =
      handler_factory_->OnNewSession(session);
  ASSERT_NE(nullptr, handler);
  // `FakeTransportSession` owns `handler`, so `CloseActiveSession()` (or
  // `~BrowserActuatorTaskClientImpl()`) destroys both `session` and `handler`
  // while `TaskIngressTransportHandler::OnMessage()` is still on the stack.
  session->SetHandler(std::move(handler));

  EXPECT_CALL(observer_, OnTaskCompleted)
      .WillOnce([this](const BrowserActuatorTaskClient::Result& result) {
        EXPECT_EQ(result.status,
                  BrowserActuatorTaskClient::CompletionStatus::kFailed);
        EXPECT_EQ(result.error_message, "server-abort");
        client_.reset();
      });

  TaskIngressDownstreamPayload completed;
  completed.mutable_task_completed()->set_status(TASK_COMPLETION_STATUS_FAILED);
  completed.mutable_task_completed()->set_error_message("server-abort");
  session->OnMessage(stream_transport::PayloadType::kTaskIngress,
                     completed.SerializeAsString());
  EXPECT_EQ(nullptr, client_);
}

TEST_F(BrowserActuatorTaskClientImplTest,
       DetachForShutdownClearsTransportServiceAndCancelsQueuedCallbacks) {
  auto heap_transport = std::make_unique<FakeTransportService>();
  auto local_client = std::make_unique<BrowserActuatorTaskClientImpl>(
      heap_transport.get(), handler_factory_->GetWeakPtr(),
      base::BindRepeating([]() { return std::string("test-client-guid"); }));

  local_client->StartTask(
      BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
      MakeSamplePayload(), &observer_);
  EXPECT_CALL(observer_, OnTaskCompleted).Times(0);

  // Queue an async completion via `Cancel()`, then run `DetachAllClients()` and
  // free `heap_transport` before `local_client` is destroyed.
  local_client->Cancel();
  handler_factory_->DetachAllClients();
  heap_transport.reset();
  task_environment_.FastForwardBy(base::TimeDelta());
  local_client.reset();
}

TEST_F(BrowserActuatorTaskClientImplTest,
       DetachForShutdownWhileStartingAbortsPendingSessionWithoutCallback) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  EXPECT_CALL(observer_, OnTaskCompleted).Times(0);

  handler_factory_->DetachAllClients();
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);
  EXPECT_TRUE(transport_service_.pending_cancelled());
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(BrowserActuatorTaskClientImplTest,
       StartTaskAfterDetachForShutdownFailsCleanly) {
  handler_factory_->DetachAllClients();
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);

  bool completed = false;
  BrowserActuatorTaskClient::Result completed_result{
      BrowserActuatorTaskClient::CompletionStatus::kSucceeded, ""};
  EXPECT_CALL(observer_, OnTaskCompleted)
      .WillOnce([&](const BrowserActuatorTaskClient::Result& result) {
        completed = true;
        completed_result = result;
      });

  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(completed);
  EXPECT_EQ(completed_result.status,
            BrowserActuatorTaskClient::CompletionStatus::kFailed);
  EXPECT_EQ(completed_result.error_message, "Transport service is shut down");
}

TEST_F(BrowserActuatorTaskClientImplTest,
       OnTaskCompletedEventIgnoredWhenNotAcceptedOrRunning) {
  // Ignored in `State::kIdle`.
  EXPECT_CALL(observer_, OnTaskCompleted).Times(0);
  client_->OnTaskCompletedEvent(TASK_COMPLETION_STATUS_FAILED, "idle");
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kIdle);

  // Ignored in `State::kStarting`.
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  ASSERT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kStarting);
  client_->OnTaskCompletedEvent(TASK_COMPLETION_STATUS_FAILED, "starting");
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kStarting);
  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  // Accepted -> Done delivers completion once; duplicate in `State::kDone` is
  // ignored.
  const std::string session_id = client_->session_id();
  transport_service_.RespondSuccess(session_id);
  ASSERT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kAccepted);

  EXPECT_CALL(
      observer_,
      OnTaskCompleted(
          AllOf(Field(&BrowserActuatorTaskClient::Result::status,
                      BrowserActuatorTaskClient::CompletionStatus::kSucceeded),
                Field(&BrowserActuatorTaskClient::Result::error_message, ""))))
      .Times(1);
  client_->OnTaskCompletedEvent(TASK_COMPLETION_STATUS_SUCCEEDED, "");
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);

  client_->OnTaskCompletedEvent(TASK_COMPLETION_STATUS_FAILED, "duplicate");
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(
    BrowserActuatorTaskClientImplTest,
    DetachAllClientsWhileActiveClosesSessionDropsLateEventsAndSurvivesTeardown) {
  auto heap_transport = std::make_unique<FakeTransportService>();
  auto local_client = std::make_unique<BrowserActuatorTaskClientImpl>(
      heap_transport.get(), handler_factory_->GetWeakPtr(),
      base::BindRepeating([]() { return std::string("test-client-guid"); }));

  local_client->StartTask(
      BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
      MakeSamplePayload(), &observer_);
  const std::string session_id = local_client->session_id();
  heap_transport->RespondSuccess(session_id);
  ASSERT_EQ(local_client->state(),
            BrowserActuatorTaskClientImpl::State::kAccepted);

  auto* session = static_cast<FakeTransportSession*>(
      heap_transport->GetSession(session_id));
  ASSERT_NE(nullptr, session);
  std::unique_ptr<stream_transport::TransportHandler> handler =
      handler_factory_->OnNewSession(session);
  ASSERT_NE(nullptr, handler);
  session->SetHandler(std::move(handler));

  // Keep a secondary handler bound before shutdown so late `OnMessage()` calls
  // also verify `TaskIngressTransportHandler`'s invalidated `WeakPtr`.
  FakeTransportSession retained_session(session_id, base::OnceClosure());
  std::unique_ptr<stream_transport::TransportHandler> retained_handler =
      handler_factory_->OnNewSession(&retained_session);
  ASSERT_NE(nullptr, retained_handler);
  retained_session.SetHandler(std::move(retained_handler));

  EXPECT_CALL(observer_, OnTaskStarted()).Times(1);
  TaskIngressDownstreamPayload started;
  started.mutable_task_started();
  session->OnMessage(stream_transport::PayloadType::kTaskIngress,
                     started.SerializeAsString());
  ASSERT_EQ(local_client->state(),
            BrowserActuatorTaskClientImpl::State::kRunning);

  EXPECT_CALL(observer_, OnWorklogUpdate(std::string_view("Running step")));
  TaskIngressDownstreamPayload worklog;
  worklog.mutable_worklog_update()->set_step_description("Running step");
  session->OnMessage(stream_transport::PayloadType::kTaskIngress,
                     worklog.SerializeAsString());
  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnTaskStarted).Times(0);
  EXPECT_CALL(observer_, OnWorklogUpdate).Times(0);
  EXPECT_CALL(observer_, OnTaskCompleted).Times(0);

  handler_factory_->DetachAllClients();
  EXPECT_EQ(local_client->state(), BrowserActuatorTaskClientImpl::State::kDone);
  EXPECT_TRUE(heap_transport->WasSessionClosed(session_id));
  EXPECT_EQ(nullptr, heap_transport->GetSession(session_id));
  FakeTransportSession probe_session(session_id, base::OnceClosure());
  EXPECT_EQ(nullptr, handler_factory_->OnNewSession(&probe_session));

  // Late downstream events via handler or direct delegate calls are dropped.
  TaskIngressDownstreamPayload late_worklog;
  late_worklog.mutable_worklog_update()->set_step_description("Late step");
  retained_session.OnMessage(stream_transport::PayloadType::kTaskIngress,
                             late_worklog.SerializeAsString());
  TaskIngressDownstreamPayload late_completed;
  late_completed.mutable_task_completed()->set_status(
      TASK_COMPLETION_STATUS_SUCCEEDED);
  retained_session.OnMessage(stream_transport::PayloadType::kTaskIngress,
                             late_completed.SerializeAsString());
  local_client->OnWorklogUpdateEvent("Direct late step");
  local_client->OnTaskCompletedEvent(TASK_COMPLETION_STATUS_SUCCEEDED, "");
  task_environment_.FastForwardBy(base::TimeDelta());
  ::testing::Mock::VerifyAndClearExpectations(&observer_);

  // `transport_service_` was cleared before `heap_transport` teardown.
  heap_transport.reset();
  local_client.reset();
}

}  // namespace
}  // namespace browser_actuator

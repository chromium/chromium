// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_task_client_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
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
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/browser_actuator/internal/task_ingress_transport_handler_factory.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/browser_actuator/public/proto/auto_password_change.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_channel.h"
#include "components/stream_transport/public/transport_handler.h"
#include "components/stream_transport/public/transport_service.h"
#include "components/stream_transport/public/transport_session.h"
#include "components/stream_transport/public/transport_session_registry.h"
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

// Mirrors `TransportSessionRegistryImpl`: `DestroySession` erases and frees the
// `FakeTransportSession` *before* iterating its non-reentrant `ObserverList`,
// and `Clear()` frees all sessions without firing `OnSessionRemoved()`.
class FakeSessionRegistry : public stream_transport::TransportSessionRegistry {
 public:
  FakeSessionRegistry() = default;
  ~FakeSessionRegistry() override = default;

  // stream_transport::TransportSessionRegistry:
  stream_transport::TransportSession* GetSession(
      std::string_view session_id) override {
    for (const auto& session : sessions_) {
      if (session->GetSessionId() == session_id) {
        return session.get();
      }
    }
    return nullptr;
  }
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }
  stream_transport::TransportSession* CreateSession(
      std::string_view session_id,
      stream_transport::InitIngressSource ingress_source) override {
    return CreateFakeSession(session_id);
  }
  stream_transport::TransportSession* CreateFakeSession(
      std::string_view session_id) {
    std::string id(session_id);
    auto session = std::make_unique<FakeTransportSession>(
        id, base::BindOnce(&FakeSessionRegistry::OnSessionClosed,
                           base::Unretained(this), id));
    FakeTransportSession* raw = session.get();
    sessions_.push_back(std::move(session));
    return raw;
  }
  void DestroySession(std::string_view session_id) override {
    std::string removed_id(session_id);
    // Erase *before* notifying observers to match production
    // `TransportSessionRegistryImpl::DestroySession()`.
    std::erase_if(sessions_, [&removed_id](const auto& s) {
      return s->GetSessionId() == removed_id;
    });
    for (auto& observer : observers_) {
      observer.OnSessionRemoved(removed_id);
    }
  }

  // Mirrors `TransportSessionRegistryImpl::Clear()` on account sign-out/switch.
  void ClearWithoutNotification() { sessions_.clear(); }

  bool WasSessionClosed(std::string_view session_id) const {
    return closed_session_ids_.contains(std::string(session_id));
  }
  bool has_observers() const { return !observers_.empty(); }

 private:
  void OnSessionClosed(const std::string& session_id) {
    closed_session_ids_.insert(session_id);
    DestroySession(session_id);
  }

  std::vector<std::unique_ptr<FakeTransportSession>> sessions_;
  std::set<std::string> closed_session_ids_;
  base::ObserverList<Observer> observers_;
};

class FakeTransportChannel : public stream_transport::TransportChannel {
 public:
  FakeTransportChannel() = default;
  ~FakeTransportChannel() override = default;

  // stream_transport::TransportChannel:
  stream_transport::TransportHandlerFactoryRegistry* GetHandlerFactoryRegistry()
      override {
    return nullptr;
  }
  stream_transport::TransportSessionRegistry* GetSessionRegistry() override {
    return &session_registry_;
  }
  void SendUpstreamMessage(
      std::string_view session_id,
      stream_transport::PayloadType payload_type,
      const google::protobuf::MessageLite& message,
      std::optional<int64_t> responding_to_sequence_number) override {}

  FakeSessionRegistry& session_registry() { return session_registry_; }
  const FakeSessionRegistry& session_registry() const {
    return session_registry_;
  }

 private:
  FakeSessionRegistry session_registry_;
};

class FakeTransportService : public stream_transport::TransportService {
 public:
  FakeTransportService() = default;
  ~FakeTransportService() override = default;

  // stream_transport::TransportService:
  stream_transport::TransportChannel* GetChannel() override {
    return &channel_;
  }
  stream_transport::TransportSession* CreateSession(
      std::string_view session_id,
      stream_transport::InitIngressSource ingress_source) override {
    return channel_.session_registry().CreateSession(session_id,
                                                     ingress_source);
  }
  stream_transport::TransportSession* GetSession(
      std::string_view session_id) override {
    return channel_.session_registry().GetSession(session_id);
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
    pending_session_ = std::make_unique<FakeTransportSession>(
        base::StrCat({"pending-", session_id}),
        base::BindOnce(&FakeTransportService::OnPendingClosed,
                       base::Unretained(this)));
    return pending_session_.get();
  }

  void RespondSuccess(std::string_view session_id) {
    stream_transport::TransportSession* matured =
        channel_.session_registry().CreateFakeSession(session_id);
    auto pending_lifetime = std::move(pending_session_);
    if (start_callback_) {
      std::move(start_callback_).Run(matured);
    }
  }

  void RespondFailure(stream_transport::StartClientSessionError error) {
    auto pending_lifetime = std::move(pending_session_);
    if (start_callback_) {
      std::move(start_callback_).Run(base::unexpected(error));
    }
  }

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
    return channel_.session_registry().WasSessionClosed(session_id);
  }
  FakeTransportChannel& channel() { return channel_; }

 private:
  void OnPendingClosed() {
    pending_cancelled_ = true;
    pending_session_.reset();
  }

  int start_call_count_ = 0;
  std::string last_session_id_;
  stream_transport::PayloadType last_payload_type_ =
      stream_transport::PayloadType::kControl;
  std::string last_serialized_message_;
  StartClientSessionCallback start_callback_;
  std::unique_ptr<FakeTransportSession> pending_session_;
  bool pending_cancelled_ = false;
  FakeTransportChannel channel_;
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
  const std::string session_id = client_->session_id();

  TaskIngressUpstreamPayload upstream_proto;
  ASSERT_TRUE(upstream_proto.ParseFromString(
      transport_service_.last_serialized_message()));
  EXPECT_EQ(upstream_proto.start_task().client_id(), "test-client-guid");
  EXPECT_EQ(
      upstream_proto.start_task().feature_payload().type_url(),
      "type.googleapis.com/browser_actuator.AutoPasswordChangeTaskPayload");

  transport_service_.RespondSuccess(session_id);
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kAccepted);
  EXPECT_NE(nullptr, transport_service_.GetSession(session_id));
}

TEST_F(BrowserActuatorTaskClientImplTest,
       RetryTransientErrorUsesExponentialBackoffAndPreservesPayload) {
  AutoPasswordChangeTaskPayload payload = MakeSamplePayload();
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     payload, &observer_);
  const std::string initial_session_id = client_->session_id();
  EXPECT_EQ(transport_service_.start_call_count(), 1);

  // First transient failure schedules a retry after
  // `kTaskIngressStartRetryDelay` (1s).
  transport_service_.RespondFailure(
      stream_transport::StartClientSessionError::kTransientError);
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kStarting);
  EXPECT_EQ(transport_service_.start_call_count(), 1);

  task_environment_.FastForwardBy(base::Milliseconds(999));
  EXPECT_EQ(transport_service_.start_call_count(), 1);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(transport_service_.start_call_count(), 2);
  EXPECT_EQ(client_->session_id(), initial_session_id);

  // Verify the retried request preserved the full payload and `client_id`.
  TaskIngressUpstreamPayload retry_proto;
  ASSERT_TRUE(retry_proto.ParseFromString(
      transport_service_.last_serialized_message()));
  AutoPasswordChangeTaskPayload unpacked;
  ASSERT_TRUE(unpacked.ParseFromString(
      retry_proto.start_task().feature_payload().value()));
  EXPECT_EQ(unpacked.domain(), "example.com");
  EXPECT_EQ(retry_proto.start_task().client_id(), "test-client-guid");

  // Second transient failure doubles backoff to 2s.
  transport_service_.RespondFailure(
      stream_transport::StartClientSessionError::kTransientError);
  task_environment_.FastForwardBy(base::Milliseconds(1999));
  EXPECT_EQ(transport_service_.start_call_count(), 2);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(transport_service_.start_call_count(), 3);

  transport_service_.RespondSuccess(initial_session_id);
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kAccepted);
}

TEST_F(BrowserActuatorTaskClientImplTest,
       ConflictAfterTransientErrorFailsWithoutRotatingSessionId) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string initial_session_id = client_->session_id();

  // Attempt 1 fails with `kTransientError` (e.g., lost 200 OK).
  transport_service_.RespondFailure(
      stream_transport::StartClientSessionError::kTransientError);
  task_environment_.FastForwardBy(kTaskIngressStartRetryDelay.Get());
  EXPECT_EQ(transport_service_.start_call_count(), 2);
  EXPECT_EQ(client_->session_id(), initial_session_id);

  // Attempt 2 receives HTTP 409 (`kSessionIdConflict`), proving Attempt 1
  // reached the server. Must NOT rotate to a second UUID.
  transport_service_.RespondFailure(
      stream_transport::StartClientSessionError::kSessionIdConflict);
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);
  EXPECT_EQ(transport_service_.start_call_count(), 2);
  EXPECT_EQ(client_->session_id(), initial_session_id);

  EXPECT_CALL(observer_,
              OnTaskCompleted(AllOf(
                  Field(&BrowserActuatorTaskClient::Result::status,
                        BrowserActuatorTaskClient::CompletionStatus::kFailed),
                  Field(&BrowserActuatorTaskClient::Result::error_message,
                        Not(IsEmpty())))));
  task_environment_.FastForwardBy(base::TimeDelta());
}

TEST_F(BrowserActuatorTaskClientImplTest,
       FirstAttemptSessionIdConflictUnregistersOldAndRetriesNewSessionId) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string old_session_id = client_->session_id();

  transport_service_.RespondFailure(
      stream_transport::StartClientSessionError::kSessionIdConflict);
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kStarting);
  EXPECT_NE(client_->session_id(), old_session_id);
  EXPECT_EQ(transport_service_.start_call_count(), 2);

  // Old `session_id` must be unregistered from `handler_factory_`.
  FakeTransportSession old_probe(old_session_id, base::OnceClosure());
  EXPECT_EQ(nullptr, handler_factory_->OnNewSession(&old_probe));

  transport_service_.RespondSuccess(client_->session_id());
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kAccepted);
}

TEST_F(BrowserActuatorTaskClientImplTest,
       CancelDuringRetryBackoffStopsTimerAndCompletesCancelled) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  transport_service_.RespondFailure(
      stream_transport::StartClientSessionError::kTransientError);
  EXPECT_EQ(transport_service_.start_call_count(), 1);

  client_->Cancel();
  EXPECT_CALL(observer_,
              OnTaskCompleted(Field(
                  &BrowserActuatorTaskClient::Result::status,
                  BrowserActuatorTaskClient::CompletionStatus::kCancelled)));
  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_EQ(transport_service_.start_call_count(), 1);
}

TEST_F(BrowserActuatorTaskClientImplTest,
       PostCompletionGraceDelayAndServerCloseDuringGraceWindowAreSafe) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  transport_service_.RespondSuccess(session_id);

  EXPECT_CALL(observer_, OnTaskStarted());
  client_->OnTaskStartedEvent();

  EXPECT_CALL(observer_,
              OnTaskCompleted(Field(
                  &BrowserActuatorTaskClient::Result::status,
                  BrowserActuatorTaskClient::CompletionStatus::kSucceeded)));
  client_->OnTaskCompletedEvent(TASK_COMPLETION_STATUS_SUCCEEDED, "");
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);
  // Session stays open during the 5s grace window so trailing `close_session`
  // can arrive from the server.
  EXPECT_NE(nullptr, transport_service_.GetSession(session_id));

  // Server sends `close_session` during the 5s window: `DestroySession` frees
  // the session *before* firing `OnSessionRemoved()`. Must not crash or fire a
  // second `OnTaskCompleted()`.
  transport_service_.channel().session_registry().DestroySession(session_id);
  EXPECT_EQ(nullptr, transport_service_.GetSession(session_id));

  task_environment_.FastForwardBy(kTaskIngressPostCompletionCloseDelay.Get());
  client_.reset();
}

TEST_F(BrowserActuatorTaskClientImplTest,
       AccountChangeSilentSessionClearDuringGraceWindowDoesNotUAF) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  transport_service_.RespondSuccess(session_id);

  EXPECT_CALL(observer_,
              OnTaskCompleted(Field(
                  &BrowserActuatorTaskClient::Result::status,
                  BrowserActuatorTaskClient::CompletionStatus::kSucceeded)));
  client_->OnTaskCompletedEvent(TASK_COMPLETION_STATUS_SUCCEEDED, "");

  // Simulate `TransportChannelImpl::ResetForAccountChange()` -> `Clear()`
  // freeing all sessions without calling `OnSessionRemoved()`.
  transport_service_.channel().session_registry().ClearWithoutNotification();
  task_environment_.FastForwardBy(kTaskIngressPostCompletionCloseDelay.Get());
  client_.reset();
}

TEST_F(BrowserActuatorTaskClientImplTest,
       UnexpectedSessionRemovedDispatchesAsyncWithoutObserverListReentrancy) {
  auto sibling_client = std::make_unique<BrowserActuatorTaskClientImpl>(
      &transport_service_, handler_factory_->GetWeakPtr(),
      base::BindRepeating([]() { return std::string("test-client-guid"); }));
  StrictMock<MockTaskClientObserver> sibling_observer;

  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_a = client_->session_id();
  transport_service_.RespondSuccess(session_a);

  sibling_client->StartTask(
      BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
      MakeSamplePayload(), &sibling_observer);
  const std::string session_b = sibling_client->session_id();
  transport_service_.RespondSuccess(session_b);

  // When `session_a` is dropped, `OnTaskCompleted` cancels `sibling_client`
  // (which destroys `session_b` in `FakeSessionRegistry`). Async dispatch
  // avoids re-entering `FakeSessionRegistry::observers_` mid-iteration.
  EXPECT_CALL(observer_,
              OnTaskCompleted(AllOf(
                  Field(&BrowserActuatorTaskClient::Result::status,
                        BrowserActuatorTaskClient::CompletionStatus::kFailed),
                  Field(&BrowserActuatorTaskClient::Result::error_message,
                        "Transport session was dropped"))))
      .WillOnce([&sibling_client](const BrowserActuatorTaskClient::Result&) {
        sibling_client->Cancel();
      });
  EXPECT_CALL(sibling_observer,
              OnTaskCompleted(Field(
                  &BrowserActuatorTaskClient::Result::status,
                  BrowserActuatorTaskClient::CompletionStatus::kCancelled)));

  transport_service_.channel().session_registry().DestroySession(session_a);
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(transport_service_.WasSessionClosed(session_b));
}

TEST_F(BrowserActuatorTaskClientImplTest,
       IdleTimeoutClosesSessionUnregistersAndResetsOnEvents) {
  client_->StartTask(BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
                     MakeSamplePayload(), &observer_);
  const std::string session_id = client_->session_id();
  transport_service_.RespondSuccess(session_id);

  // Advance 4 minutes in `kAccepted`, then `OnTaskStartedEvent` resets the 5m
  // watchdog.
  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_CALL(observer_, OnTaskStarted());
  client_->OnTaskStartedEvent();

  // Advance another 4 minutes in `kRunning`, then `OnWorklogUpdateEvent` resets
  // the 5m watchdog again.
  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_CALL(observer_, OnWorklogUpdate(std::string_view("Halfway")));
  client_->OnWorklogUpdateEvent("Halfway");

  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kRunning);

  EXPECT_CALL(observer_,
              OnTaskCompleted(AllOf(
                  Field(&BrowserActuatorTaskClient::Result::status,
                        BrowserActuatorTaskClient::CompletionStatus::kFailed),
                  Field(&BrowserActuatorTaskClient::Result::error_message,
                        "Task timed out waiting for progress"))));
  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(client_->state(), BrowserActuatorTaskClientImpl::State::kDone);
  EXPECT_TRUE(transport_service_.WasSessionClosed(session_id));
  FakeTransportSession probe(session_id, base::OnceClosure());
  EXPECT_EQ(nullptr, handler_factory_->OnNewSession(&probe));
}

TEST_F(BrowserActuatorTaskClientImplTest,
       NullHandlerFactorySkipsObservationAndFailsCleanlyOnStart) {
  client_.reset();
  ASSERT_FALSE(transport_service_.channel().session_registry().has_observers());
  auto untracked_client = std::make_unique<BrowserActuatorTaskClientImpl>(
      &transport_service_, /*handler_factory=*/nullptr,
      base::BindRepeating([]() { return std::string("test-client-guid"); }));
  EXPECT_FALSE(transport_service_.channel().session_registry().has_observers());

  untracked_client->StartTask(
      BrowserActuatorTaskClient::UseCase::kAutoPasswordChange,
      MakeSamplePayload(), &observer_);
  EXPECT_EQ(untracked_client->state(),
            BrowserActuatorTaskClientImpl::State::kDone);
  EXPECT_CALL(observer_,
              OnTaskCompleted(
                  Field(&BrowserActuatorTaskClient::Result::status,
                        BrowserActuatorTaskClient::CompletionStatus::kFailed)));
  task_environment_.FastForwardBy(base::TimeDelta());
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

  local_client->Cancel();
  handler_factory_->DetachAllClients();
  heap_transport.reset();
  task_environment_.FastForwardBy(base::TimeDelta());
  local_client.reset();
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

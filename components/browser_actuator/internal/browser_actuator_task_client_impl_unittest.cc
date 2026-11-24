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

  // stream_transport::TransportSession:
  std::string_view GetSessionId() const override { return session_id_; }

  base::expected<void, stream_transport::SendUpstreamMessageError>
  SendUpstreamMessage(stream_transport::PayloadType payload_type,
                      const google::protobuf::MessageLite& message) override {
    return base::ok();
  }

  void OnMessage(stream_transport::PayloadType payload_type,
                 std::string_view serialized_payload) override {}

  void Close() override {
    if (on_close_) {
      std::move(on_close_).Run();
    }
  }

 private:
  std::string session_id_;
  base::OnceClosure on_close_;
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

}  // namespace
}  // namespace browser_actuator

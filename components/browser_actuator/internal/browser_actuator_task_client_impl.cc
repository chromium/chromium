// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_task_client_impl.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/browser_actuator/internal/task_ingress_transport_handler_factory.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/browser_actuator/public/features.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/payload_type_mapping.h"
#include "components/stream_transport/public/transport_channel.h"
#include "components/stream_transport/public/transport_service.h"
#include "components/stream_transport/public/transport_session.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace browser_actuator {

BASE_FEATURE_PARAM(int,
                   kTaskIngressMaxStartRetries,
                   &kBrowserActuatorTaskIngress,
                   2);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kTaskIngressStartRetryDelay,
                   &kBrowserActuatorTaskIngress,
                   base::Seconds(1));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kTaskIngressIdleTimeout,
                   &kBrowserActuatorTaskIngress,
                   base::Minutes(5));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kTaskIngressPostCompletionCloseDelay,
                   &kBrowserActuatorTaskIngress,
                   base::Seconds(5));

namespace {

constexpr int kMaxBackoffShift = 6;

TaskUseCase ToProtoTaskUseCase(BrowserActuatorTaskClient::UseCase use_case) {
  switch (use_case) {
    case BrowserActuatorTaskClient::UseCase::kAutoPasswordChange:
      return TASK_USE_CASE_AUTO_PASSWORD_CHANGE;
  }
  NOTREACHED();
}

std::string_view FeaturePayloadTypeUrlPrefix(
    BrowserActuatorTaskClient::UseCase use_case) {
  switch (use_case) {
    case BrowserActuatorTaskClient::UseCase::kAutoPasswordChange:
      return stream_transport::kTypeUrlPrefix;
  }
  NOTREACHED();
}

struct PackedPayload {
  std::string type_url;
  std::string value;
};

PackedPayload PackFeaturePayload(BrowserActuatorTaskClient::UseCase use_case,
                                 const google::protobuf::MessageLite& payload) {
  constexpr char kAnyProtoTypeName[] = "google.protobuf.Any";
  if (payload.GetTypeName() == kAnyProtoTypeName) {
    StartTaskCommand wrapper;
    if (wrapper.mutable_feature_payload()->ParseFromString(
            payload.SerializeAsString())) {
      return {std::move(*wrapper.mutable_feature_payload()->mutable_type_url()),
              std::move(*wrapper.mutable_feature_payload()->mutable_value())};
    }
  }
  PackedPayload packed;
  packed.type_url = base::StrCat(
      {FeaturePayloadTypeUrlPrefix(use_case), payload.GetTypeName()});
  payload.SerializeToString(&packed.value);
  return packed;
}

BrowserActuatorTaskClient::CompletionStatus ToClientStatus(
    TaskCompletionStatus status) {
  switch (status) {
    case TASK_COMPLETION_STATUS_SUCCEEDED:
      return BrowserActuatorTaskClient::CompletionStatus::kSucceeded;
    case TASK_COMPLETION_STATUS_FAILED:
    case TASK_COMPLETION_STATUS_UNSPECIFIED:
      return BrowserActuatorTaskClient::CompletionStatus::kFailed;
    case TASK_COMPLETION_STATUS_CANCELLED:
      return BrowserActuatorTaskClient::CompletionStatus::kCancelled;
    default:
      return BrowserActuatorTaskClient::CompletionStatus::kFailed;
  }
}

std::string DescribeStartSessionError(
    stream_transport::StartClientSessionError error) {
  switch (error) {
    case stream_transport::StartClientSessionError::kChannelUnavailable:
      return "Transport channel is unavailable";
    case stream_transport::StartClientSessionError::kSessionLimitReached:
      return "Active transport session limit reached";
    case stream_transport::StartClientSessionError::kSessionIdConflict:
      return "Transport session ID conflict";
    case stream_transport::StartClientSessionError::kTransientError:
      return "Transient transport session start error";
    case stream_transport::StartClientSessionError::kPermanentError:
      return "Permanent transport session start error";
  }
  NOTREACHED();
}

}  // namespace

BrowserActuatorTaskClientImpl::BrowserActuatorTaskClientImpl(
    stream_transport::TransportService* transport_service,
    HandlerFactory handler_factory,
    DeviceIdProvider device_id_provider)
    : handler_factory_(std::move(handler_factory)),
      device_id_provider_(std::move(device_id_provider)),
      transport_service_(transport_service) {
  CHECK(transport_service_);
  CHECK(device_id_provider_);
  if (handler_factory_) {
    handler_factory_->TrackClient(weak_ptr_factory_.GetWeakPtr());
    if (stream_transport::TransportChannel* channel =
            transport_service_->GetChannel();
        channel && channel->GetSessionRegistry()) {
      session_registry_observation_.Observe(channel->GetSessionRegistry());
    }
  }
}

BrowserActuatorTaskClientImpl::~BrowserActuatorTaskClientImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_registry_observation_.Reset();
  StopAllTimers();
  if (state_ != State::kDone) {
    UnregisterSession();
  }
  ClosePendingSession();
  CloseActiveSession();
}

void BrowserActuatorTaskClientImpl::StartTask(
    UseCase use_case,
    const google::protobuf::MessageLite& payload,
    BrowserActuatorTaskClient::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!start_task_called_);
  start_task_called_ = true;
  CHECK(observer);
  observer_ = observer;
  if (!transport_service_ || !handler_factory_) {
    state_ = State::kDone;
    NotifyTaskCompleted(
        {CompletionStatus::kFailed, "Transport service is shut down"});
    return;
  }
  CHECK_EQ(state_, State::kIdle);
  client_id_ = device_id_provider_.Run();
  if (client_id_.empty()) {
    state_ = State::kDone;
    NotifyTaskCompleted(
        {CompletionStatus::kFailed, "Sync device ID is unavailable"});
    return;
  }
  use_case_ = use_case;
  PackedPayload packed = PackFeaturePayload(use_case, payload);
  type_url_ = std::move(packed.type_url);
  payload_ = std::move(packed.value);
  start_retries_remaining_ = std::max(0, kTaskIngressMaxStartRetries.Get());
  transient_retry_count_ = 0;
  current_session_had_transient_error_ = false;
  session_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
  SendStartRequest();
}

void BrowserActuatorTaskClientImpl::SendStartRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(transport_service_);
  state_ = State::kStarting;
  if (handler_factory_) {
    handler_factory_->RegisterSessionClient(session_id_,
                                            weak_ptr_factory_.GetWeakPtr());
  }
  TaskIngressUpstreamPayload payload;
  StartTaskCommand* cmd = payload.mutable_start_task();
  cmd->set_use_case(ToProtoTaskUseCase(use_case_));
  cmd->mutable_feature_payload()->set_type_url(type_url_);
  cmd->mutable_feature_payload()->set_value(payload_);
  cmd->set_client_id(client_id_);
  pending_session_ = transport_service_->StartClientSession(
      session_id_, stream_transport::PayloadType::kTaskIngress, payload,
      base::BindOnce(&BrowserActuatorTaskClientImpl::OnSessionStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowserActuatorTaskClientImpl::OnSessionStarted(
    base::expected<stream_transport::TransportSession*,
                   stream_transport::StartClientSessionError> res) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_session_ = nullptr;
  if (state_ != State::kStarting) {
    if (res.has_value()) {
      CHECK(res.value());
      res.value()->Close();
    }
    return;
  }
  if (res.has_value()) {
    CHECK(res.value());
    ClearStartupBuffers();
    has_active_session_ = true;
    state_ = State::kAccepted;
    ResetIdleTimer();
    return;
  }
  if (start_retries_remaining_ > 0) {
    switch (res.error()) {
      case stream_transport::StartClientSessionError::kTransientError: {
        --start_retries_remaining_;
        current_session_had_transient_error_ = true;
        int shift = std::min(transient_retry_count_++, kMaxBackoffShift);
        base::TimeDelta base_delay =
            std::max(base::TimeDelta(), kTaskIngressStartRetryDelay.Get());
        start_retry_timer_.Start(
            FROM_HERE, base_delay * (1 << shift),
            base::BindOnce(&BrowserActuatorTaskClientImpl::SendStartRequest,
                           weak_ptr_factory_.GetWeakPtr()));
        return;
      }
      case stream_transport::StartClientSessionError::kSessionIdConflict:
        // If this `session_id_` already experienced a `kTransientError`, the
        // earlier attempt may have succeeded on the server with a lost 200 OK.
        // Rotating to a new UUID would spawn a duplicate Agency session.
        if (current_session_had_transient_error_) {
          break;
        }
        --start_retries_remaining_;
        UnregisterSession();
        session_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
        SendStartRequest();
        return;
      case stream_transport::StartClientSessionError::kChannelUnavailable:
      case stream_transport::StartClientSessionError::kSessionLimitReached:
      case stream_transport::StartClientSessionError::kPermanentError:
        break;
    }
  }
  state_ = State::kDone;
  session_registry_observation_.Reset();
  ClearStartupBuffers();
  UnregisterSession();
  NotifyTaskCompleted(
      {CompletionStatus::kFailed, DescribeStartSessionError(res.error())});
}

void BrowserActuatorTaskClientImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kIdle || state_ == State::kDone) {
    return;
  }
  state_ = State::kDone;
  session_registry_observation_.Reset();
  StopAllTimers();
  ClearStartupBuffers();
  UnregisterSession();
  ClosePendingSession();
  CloseActiveSession();
  NotifyTaskCompleted({CompletionStatus::kCancelled, ""});
}

void BrowserActuatorTaskClientImpl::OnTaskStartedEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kAccepted) {
    state_ = State::kRunning;
    ResetIdleTimer();
    if (observer_) {
      observer_->OnTaskStarted();
    }
  }
}

void BrowserActuatorTaskClientImpl::OnWorklogUpdateEvent(
    std::string_view step_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kRunning && observer_) {
    ResetIdleTimer();
    observer_->OnWorklogUpdate(step_description);
  }
}

void BrowserActuatorTaskClientImpl::OnTaskCompletedEvent(
    TaskCompletionStatus status,
    std::string_view error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kAccepted && state_ != State::kRunning) {
    return;
  }
  state_ = State::kDone;
  idle_timer_.Stop();
  UnregisterSession();
  if (base::TimeDelta delay = kTaskIngressPostCompletionCloseDelay.Get();
      delay.is_positive()) {
    post_completion_close_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(
            &BrowserActuatorTaskClientImpl::OnPostCompletionCloseTimeout,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnPostCompletionCloseTimeout();
  }
  if (BrowserActuatorTaskClient::Observer* obs =
          std::exchange(observer_, nullptr)) {
    obs->OnTaskCompleted({ToClientStatus(status), std::string(error_message)});
  }
}

void BrowserActuatorTaskClientImpl::OnPostCompletionCloseTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_registry_observation_.Reset();
  CloseActiveSession();
}

void BrowserActuatorTaskClientImpl::DetachForShutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kDone;
  observer_ = nullptr;
  weak_ptr_factory_.InvalidateWeakPtrs();
  session_registry_observation_.Reset();
  StopAllTimers();
  ClearStartupBuffers();
  UnregisterSession();
  ClosePendingSession();
  CloseActiveSession();
  transport_service_ = nullptr;
}

void BrowserActuatorTaskClientImpl::OnSessionRemoved(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_id != session_id_) {
    return;
  }
  pending_session_ = nullptr;
  has_active_session_ = false;
  post_completion_close_timer_.Stop();
  session_registry_observation_.Reset();
  if (state_ == State::kAccepted || state_ == State::kRunning) {
    state_ = State::kDone;
    idle_timer_.Stop();
    UnregisterSession();
    NotifyTaskCompleted(
        {CompletionStatus::kFailed, "Transport session was dropped"});
  }
}

BrowserActuatorTaskClientImpl::State BrowserActuatorTaskClientImpl::state()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

const std::string& BrowserActuatorTaskClientImpl::session_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_id_;
}

void BrowserActuatorTaskClientImpl::ClearStartupBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  type_url_ = {};
  payload_ = {};
  client_id_ = {};
}

void BrowserActuatorTaskClientImpl::StopAllTimers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  start_retry_timer_.Stop();
  idle_timer_.Stop();
  post_completion_close_timer_.Stop();
}

void BrowserActuatorTaskClientImpl::UnregisterSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (handler_factory_ && !session_id_.empty()) {
    handler_factory_->UnregisterSessionClient(session_id_);
  }
}

void BrowserActuatorTaskClientImpl::ClosePendingSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_transport::TransportSession* pending = pending_session_;
  pending_session_ = nullptr;
  if (pending) {
    pending->Close();
  }
}

void BrowserActuatorTaskClientImpl::CloseActiveSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!std::exchange(has_active_session_, false) || !transport_service_ ||
      session_id_.empty()) {
    return;
  }
  if (stream_transport::TransportSession* session =
          transport_service_->GetSession(session_id_)) {
    session->Close();
  }
}

void BrowserActuatorTaskClientImpl::NotifyTaskCompleted(Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (BrowserActuatorTaskClient::Observer* obs =
          std::exchange(observer_, nullptr)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserActuatorTaskClientImpl::DispatchTaskCompleted,
                       weak_ptr_factory_.GetWeakPtr(), obs, std::move(result)));
  }
}

void BrowserActuatorTaskClientImpl::DispatchTaskCompleted(
    BrowserActuatorTaskClient::Observer* observer,
    Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(observer);
  observer->OnTaskCompleted(result);
}

void BrowserActuatorTaskClientImpl::ResetIdleTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::TimeDelta timeout = kTaskIngressIdleTimeout.Get();
      timeout.is_positive()) {
    idle_timer_.Start(
        FROM_HERE, timeout,
        base::BindOnce(&BrowserActuatorTaskClientImpl::OnIdleTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void BrowserActuatorTaskClientImpl::OnIdleTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kDone) {
    return;
  }
  state_ = State::kDone;
  session_registry_observation_.Reset();
  UnregisterSession();
  CloseActiveSession();
  if (BrowserActuatorTaskClient::Observer* obs =
          std::exchange(observer_, nullptr)) {
    obs->OnTaskCompleted(
        {CompletionStatus::kFailed, "Task timed out waiting for progress"});
  }
}

}  // namespace browser_actuator

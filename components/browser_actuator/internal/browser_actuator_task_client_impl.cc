// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_task_client_impl.h"

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
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/payload_type_mapping.h"
#include "components/stream_transport/public/transport_service.h"
#include "components/stream_transport/public/transport_session.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace browser_actuator {

namespace {

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
}

BrowserActuatorTaskClientImpl::~BrowserActuatorTaskClientImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClosePendingSession();
  CloseActiveSession();
}

void BrowserActuatorTaskClientImpl::StartTask(
    UseCase use_case,
    const google::protobuf::MessageLite& payload,
    BrowserActuatorTaskClient::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kIdle);
  CHECK(observer);
  observer_ = observer;
  if (!transport_service_) {
    state_ = State::kDone;
    NotifyTaskCompleted(
        {CompletionStatus::kFailed, "Transport service is shut down"});
    return;
  }
  std::string client_id = device_id_provider_.Run();
  if (client_id.empty()) {
    state_ = State::kDone;
    NotifyTaskCompleted(
        {CompletionStatus::kFailed, "Sync device ID is unavailable"});
    return;
  }
  auto [type_url, serialized_payload] = PackFeaturePayload(use_case, payload);
  SendStartRequest(use_case, std::move(type_url), std::move(serialized_payload),
                   std::move(client_id));
}

void BrowserActuatorTaskClientImpl::SendStartRequest(
    UseCase use_case,
    std::string type_url,
    std::string serialized_payload,
    std::string client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(transport_service_);
  state_ = State::kStarting;
  session_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
  TaskIngressUpstreamPayload payload;
  StartTaskCommand* cmd = payload.mutable_start_task();
  cmd->set_use_case(ToProtoTaskUseCase(use_case));
  cmd->mutable_feature_payload()->set_type_url(std::move(type_url));
  cmd->mutable_feature_payload()->set_value(std::move(serialized_payload));
  cmd->set_client_id(std::move(client_id));
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
    has_active_session_ = true;
    state_ = State::kAccepted;
  } else {
    state_ = State::kDone;
    NotifyTaskCompleted(
        {CompletionStatus::kFailed, "Failed to start transport session"});
  }
}

void BrowserActuatorTaskClientImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kIdle || state_ == State::kDone) {
    return;
  }
  state_ = State::kDone;
  ClosePendingSession();
  CloseActiveSession();
  NotifyTaskCompleted({CompletionStatus::kCancelled, ""});
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

}  // namespace browser_actuator

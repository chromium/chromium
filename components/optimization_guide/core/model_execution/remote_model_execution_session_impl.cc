// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/remote_model_execution_session_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/access_token_helper.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/remote_model_execution_common.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace {

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

}  // namespace

GURL GetModelExecutionServiceStreamURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          kOptimizationGuideServiceModelExecutionStreamURLSwitch)) {
    return GURL(command_line->GetSwitchValueASCII(
        kOptimizationGuideServiceModelExecutionStreamURLSwitch));
  }
  return GURL(kOptimizationGuideServiceModelExecutionDefaultStreamURL);
}

RemoteModelExecutionSessionImpl::RemoteModelExecutionSessionImpl(
    ModelBasedCapabilityKey feature,
    const StreamingModelExecutionOptions& options,
    OptimizationGuideModelExecutionStreamingCallback callback,
    network::mojom::NetworkContext* network_context,
    signin::IdentityManager* identity_manager,
    OptimizationGuideLogger* logger)
    : RemoteModelExecutionSessionImpl(
          feature,
          options,
          std::move(callback),
          identity_manager,
          std::make_unique<streaming_client::StreamingWebSocketClient>(
              GetModelExecutionServiceStreamURL(),
              network_context,
              GetNetworkTrafficAnnotation(feature),
              /*delegate=*/this),
          logger) {}

RemoteModelExecutionSessionImpl::RemoteModelExecutionSessionImpl(
    ModelBasedCapabilityKey feature,
    const StreamingModelExecutionOptions& options,
    OptimizationGuideModelExecutionStreamingCallback callback,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<streaming_client::StreamingWebSocketClient> client,
    OptimizationGuideLogger* logger)
    : feature_(feature),
      options_(options),
      callback_(std::move(callback)),
      identity_manager_(identity_manager),
      optimization_guide_logger_(logger),
      client_(std::move(client)) {
  CHECK(callback_);
  CHECK(client_);
  // TODO(crbug.com/553134125): Refactor out `set_delegate` call as it is only
  // needed for tests.
  client_->set_delegate(this);
  if (options_.prewarm_connection) {
    StartConnection();
  }
}

RemoteModelExecutionSessionImpl::~RemoteModelExecutionSessionImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  idle_timer_.Stop();
  client_.reset();
}

void RemoteModelExecutionSessionImpl::Send(
    const google::protobuf::MessageLite& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  proto::ExecuteRequest execute_request =
      CreateExecuteRequest(feature_, message);

  std::string serialized_request;
  if (!execute_request.SerializeToString(&serialized_request)) {
    DispatchError(OptimizationGuideModelExecutionError::FromModelExecutionError(
        ModelExecutionError::kGenericFailure));
    return;
  }

  std::vector<uint8_t> request_bytes(serialized_request.begin(),
                                     serialized_request.end());

  if (connection_state_ == ConnectionState::kConnected) {
    ResetIdleTimer();
    client_->Send(std::move(request_bytes));
    return;
  }

  pending_requests_.push_back(std::move(request_bytes));
  StartConnection();
}

void RemoteModelExecutionSessionImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void RemoteModelExecutionSessionImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void RemoteModelExecutionSessionImpl::StartConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (connection_state_ == ConnectionState::kConnecting ||
      connection_state_ == ConnectionState::kConnected) {
    return;
  }

  SetConnectionState(ConnectionState::kConnecting);

  if (IsAccessTokenRequiredForFeature(feature_) && access_token_.empty()) {
    HandleTokenRequestFlow(
        /*require_token=*/true, identity_manager_,
        signin::OAuthConsumerId::kOptimizationGuideModelExecution,
        base::BindOnce(&RemoteModelExecutionSessionImpl::OnAccessTokenReceived,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  client_->Connect();
}

void RemoteModelExecutionSessionImpl::OnAccessTokenReceived(
    const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Return early if the session state changed after requesting the access
  // token (e.g. disconnected while access token is pending).
  if (connection_state_ != ConnectionState::kConnecting) {
    return;
  }

  if (IsAccessTokenRequiredForFeature(feature_) && access_token.empty() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          kOptimizationGuideServiceModelExecutionStreamURLSwitch)) {
    HandleDisconnection(
        OptimizationGuideModelExecutionError::FromModelExecutionError(
            ModelExecutionError::kPermissionDenied));
    return;
  }

  access_token_ = access_token;
  client_->Connect();
}

void RemoteModelExecutionSessionImpl::SendPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::vector<uint8_t>> pending_requests =
      std::exchange(pending_requests_, {});
  for (std::vector<uint8_t>& request : pending_requests) {
    if (connection_state_ != ConnectionState::kConnected) {
      break;
    }
    client_->Send(std::move(request));
  }
}

void RemoteModelExecutionSessionImpl::OnConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetConnectionState(ConnectionState::kConnected);
  ResetIdleTimer();
  SendPendingRequests();
}

void RemoteModelExecutionSessionImpl::OnMessage(std::vector<uint8_t> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetIdleTimer();

  proto::ExecuteResponse execute_response;
  if (!execute_response.ParseFromArray(message.data(), message.size())) {
    DispatchError(OptimizationGuideModelExecutionError::FromModelExecutionError(
        ModelExecutionError::kGenericFailure));
    return;
  }

  // TODO(crbug.com/553134125): Refactor the logic populating execution_info
  // into a shared utility with the unary path.
  std::unique_ptr<proto::ModelExecutionInfo> execution_info;
  if (execute_response.has_server_execution_id()) {
    execution_info = std::make_unique<proto::ModelExecutionInfo>();
    execution_info->set_execution_id(execute_response.server_execution_id());
  }

  if (execute_response.has_error_response()) {
    if (!execution_info) {
      execution_info = std::make_unique<proto::ModelExecutionInfo>();
    }
    *execution_info->mutable_error_response() =
        execute_response.error_response();
    auto error =
        OptimizationGuideModelExecutionError::FromModelExecutionServerError(
            execute_response.error_response());
    execution_info->set_model_execution_error_enum(
        static_cast<uint32_t>(error.error()));
    DispatchResponse(OptimizationGuideModelStreamingResult(
        base::unexpected(error), std::move(execution_info)));
    return;
  }

  if (!execute_response.has_response_metadata()) {
    if (!execution_info) {
      execution_info = std::make_unique<proto::ModelExecutionInfo>();
    }
    auto error = OptimizationGuideModelExecutionError::FromModelExecutionError(
        ModelExecutionError::kGenericFailure);
    execution_info->set_model_execution_error_enum(
        static_cast<uint32_t>(error.error()));
    DispatchResponse(OptimizationGuideModelStreamingResult(
        base::unexpected(error), std::move(execution_info)));
    return;
  }

  DispatchResponse(OptimizationGuideModelStreamingResult(
      base::ok(std::move(*execute_response.mutable_response_metadata())),
      std::move(execution_info)));
}

void RemoteModelExecutionSessionImpl::OnConnectionError(
    const std::string& message,
    int net_error,
    int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleDisconnection(
      response_code > 0
          ? OptimizationGuideModelExecutionError::FromHttpStatusCode(
                static_cast<net::HttpStatusCode>(response_code))
          : OptimizationGuideModelExecutionError::FromModelExecutionError(
                ModelExecutionError::kGenericFailure));
}

void RemoteModelExecutionSessionImpl::OnDropChannel(
    bool was_clean,
    uint16_t code,
    const std::string& reason,
    std::optional<base::TimeDelta> elapsed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleDisconnection(
      was_clean
          ? std::nullopt
          : std::make_optional(
                OptimizationGuideModelExecutionError::FromModelExecutionError(
                    ModelExecutionError::kGenericFailure)));
}

void RemoteModelExecutionSessionImpl::OnError(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleDisconnection(
      OptimizationGuideModelExecutionError::FromModelExecutionError(
          ModelExecutionError::kGenericFailure));
}

void RemoteModelExecutionSessionImpl::OnClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleDisconnection(
      OptimizationGuideModelExecutionError::FromModelExecutionError(
          ModelExecutionError::kGenericFailure));
}

std::vector<network::mojom::HttpHeaderPtr>
RemoteModelExecutionSessionImpl::GetAdditionalHeaders() {
  std::vector<network::mojom::HttpHeaderPtr> additional_headers;
  if (!access_token_.empty()) {
    additional_headers.push_back(network::mojom::HttpHeader::New(
        "Authorization", "Bearer " + access_token_));
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kModelExecutionEnableRemoteDebugLoggingSwitch)) {
    additional_headers.push_back(network::mojom::HttpHeader::New(
        kOptimizationGuideModelExecutionDebugLogsHeaderKey, ""));
  }
  return additional_headers;
}

void RemoteModelExecutionSessionImpl::SetConnectionState(
    ConnectionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state == ConnectionState::kDisconnected) {
    pending_requests_.clear();
    access_token_.clear();
  }
  if (connection_state_ == state) {
    return;
  }
  connection_state_ = state;
  for (Observer& observer : observers_) {
    observer.OnConnectionStateChanged(connection_state_);
  }
}

void RemoteModelExecutionSessionImpl::HandleDisconnection(
    std::optional<OptimizationGuideModelExecutionError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  idle_timer_.Stop();
  SetConnectionState(ConnectionState::kDisconnected);
  if (error) {
    DispatchError(*error);
  }
}

void RemoteModelExecutionSessionImpl::ResetIdleTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (options_.idle_disconnect_timeout.is_positive()) {
    idle_timer_.Start(
        FROM_HERE, options_.idle_disconnect_timeout,
        base::BindRepeating(&RemoteModelExecutionSessionImpl::OnIdleTimeout,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void RemoteModelExecutionSessionImpl::OnIdleTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->Close();
  HandleDisconnection(/*error=*/std::nullopt);
}

void RemoteModelExecutionSessionImpl::DispatchResponse(
    OptimizationGuideModelStreamingResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(callback_, std::move(result)));
}

void RemoteModelExecutionSessionImpl::DispatchError(
    OptimizationGuideModelExecutionError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto execution_info = std::make_unique<proto::ModelExecutionInfo>();
  execution_info->set_model_execution_error_enum(
      static_cast<uint32_t>(error.error()));
  DispatchResponse(OptimizationGuideModelStreamingResult(
      base::unexpected(error), std::move(execution_info)));
}

}  // namespace optimization_guide

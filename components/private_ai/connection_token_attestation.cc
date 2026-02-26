// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_token_attestation.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace private_ai {

ConnectionTokenAttestation::PendingRequest::PendingRequest(
    proto::PrivateAiRequest request,
    base::TimeDelta timeout,
    OnRequestCallback callback)
    : request(std::move(request)),
      timeout(timeout),
      callback(std::move(callback)) {}

ConnectionTokenAttestation::PendingRequest::~PendingRequest() = default;

ConnectionTokenAttestation::PendingRequest::PendingRequest(PendingRequest&&) =
    default;

ConnectionTokenAttestation::PendingRequest&
ConnectionTokenAttestation::PendingRequest::operator=(PendingRequest&&) =
    default;

ConnectionTokenAttestation::ConnectionTokenAttestation(
    std::unique_ptr<Connection> inner_connection,
    phosphor::TokenManager* token_manager,
    PrivateAiLogger* logger,
    base::OnceCallback<void(ErrorCode)> on_disconnect)
    : inner_connection_(std::move(inner_connection)),
      token_manager_(token_manager),
      logger_(logger),
      on_disconnect_(std::move(on_disconnect)) {
  CHECK(inner_connection_);
  CHECK(token_manager_);
  CHECK(logger_);
  CHECK(on_disconnect_);

  // Sending attestation request asynchronously to avoid failures
  // during construction of this object.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ConnectionTokenAttestation::FetchToken,
                                weak_factory_.GetWeakPtr()));
}

ConnectionTokenAttestation::~ConnectionTokenAttestation() = default;

void ConnectionTokenAttestation::Send(proto::PrivateAiRequest request,
                                      base::TimeDelta timeout,
                                      OnRequestCallback callback) {
  if (attestation_state_ == AttestationState::kSuccess) {
    inner_connection_->Send(std::move(request), timeout, std::move(callback));
    return;
  }

  if (attestation_state_ == AttestationState::kFailed) {
    std::move(callback).Run(
        base::unexpected(ErrorCode::kClientAttestationFailed));
    return;
  }

  pending_requests_.emplace_back(std::move(request), timeout,
                                 std::move(callback));
}

void ConnectionTokenAttestation::FetchToken() {
  attestation_state_ = AttestationState::kFetchingToken;
  token_manager_->GetAuthToken(base::BindOnce(
      &ConnectionTokenAttestation::OnTokenFetched, weak_factory_.GetWeakPtr()));
}

void ConnectionTokenAttestation::OnTokenFetched(
    std::optional<phosphor::BlindSignedAuthToken> auth_token) {
  if (!auth_token.has_value()) {
    logger_->LogError(FROM_HERE, "Failed to get anonymous auth token");
    CallOnDisconnect(ErrorCode::kClientAttestationFailed);
    return;
  }

  attestation_state_ = AttestationState::kWaitingAttestationResponse;

  proto::PrivateAiRequest request_proto;
  request_proto.mutable_anonymous_token_request()->set_anonymous_token(
      auth_token->token);
  request_proto.mutable_anonymous_token_request()->set_encoded_extensions(
      auth_token->encoded_extensions);

  logger_->LogInfo(FROM_HERE, "Sending auth token");
  inner_connection_->Send(
      std::move(request_proto), base::Seconds(60),
      base::BindOnce(&ConnectionTokenAttestation::OnAttestationResponse,
                     weak_factory_.GetWeakPtr()));
}

void ConnectionTokenAttestation::OnAttestationResponse(
    base::expected<proto::PrivateAiResponse, ErrorCode> result) {
  if (!result.has_value()) {
    logger_->LogError(
        FROM_HERE,
        base::StrCat({"Client attestation request failed with error: ",
                      base::NumberToString(static_cast<int>(result.error()))}));
    base::UmaHistogramEnumeration("PrivateAi.Client.RequestErrorCode",
                                  result.error());
    CallOnDisconnect(ErrorCode::kClientAttestationFailed);
    return;
  }

  attestation_state_ = AttestationState::kSuccess;
  auto pending_requests = std::move(pending_requests_);
  for (auto& pending_request : pending_requests) {
    inner_connection_->Send(std::move(pending_request.request),
                            pending_request.timeout,
                            std::move(pending_request.callback));
  }
}

void ConnectionTokenAttestation::OnDestroy(ErrorCode error) {
  attestation_state_ = AttestationState::kFailed;
  on_disconnect_.Reset();

  auto pending_requests = std::move(pending_requests_);
  for (auto& pending_request : pending_requests) {
    std::move(pending_request.callback).Run(base::unexpected(error));
  }

  inner_connection_->OnDestroy(error);

  token_manager_ = nullptr;
  logger_ = nullptr;
  weak_factory_.InvalidateWeakPtrsAndDoom();
}

void ConnectionTokenAttestation::CallOnDisconnect(ErrorCode error_code) {
  if (on_disconnect_) {
    std::move(on_disconnect_).Run(error_code);
  }
}

}  // namespace private_ai

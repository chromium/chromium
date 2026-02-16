// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/connection_token_attestation.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/legion/phosphor/token_manager.h"
#include "components/legion/proto/legion.pb.h"

namespace private_ai {

ConnectionTokenAttestation::PendingRequest::PendingRequest(
    proto::LegionRequest request,
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
    base::OnceClosure on_disconnect)
    : inner_connection_(std::move(inner_connection)),
      token_manager_(token_manager),
      on_disconnect_(std::move(on_disconnect)) {
  CHECK(inner_connection_);
  CHECK(token_manager_);
  CHECK(on_disconnect_);

  // Sending attestation request as soon as possible.
  FetchToken();
}

ConnectionTokenAttestation::~ConnectionTokenAttestation() = default;

void ConnectionTokenAttestation::Send(proto::LegionRequest request,
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
  token_manager_->GetAuthToken(
      base::BindOnce(&ConnectionTokenAttestation::OnTokenFetched,
                     weak_factory_.GetWeakPtr()));
}

void ConnectionTokenAttestation::OnTokenFetched(
    std::optional<phosphor::BlindSignedAuthToken> auth_token) {
  if (!auth_token.has_value()) {
    LOG(ERROR) << "Failed to get anonymous auth token";
    FailPendingRequestsAndCallOnDisconnect(ErrorCode::kClientAttestationFailed);
    return;
  }

  attestation_state_ = AttestationState::kWaitingAttestationResponse;

  proto::LegionRequest request_proto;
  request_proto.mutable_anonymous_token_request()->set_anonymous_token(
      auth_token->token);
  request_proto.mutable_anonymous_token_request()->set_encoded_extensions(
      auth_token->encoded_extensions);

  inner_connection_->Send(
      std::move(request_proto), base::Seconds(60),
      base::BindOnce(&ConnectionTokenAttestation::OnAttestationResponse,
                     weak_factory_.GetWeakPtr()));
}

void ConnectionTokenAttestation::OnAttestationResponse(
    base::expected<proto::LegionResponse, ErrorCode> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Client attestation request failed with error: "
               << static_cast<int>(result.error());
    base::UmaHistogramEnumeration("Legion.Client.RequestErrorCode",
                                  result.error());
    FailPendingRequestsAndCallOnDisconnect(ErrorCode::kClientAttestationFailed);
    return;
  }

  attestation_state_ = AttestationState::kSuccess;
  for (auto& pending_request : pending_requests_) {
    inner_connection_->Send(std::move(pending_request.request),
                            pending_request.timeout,
                            std::move(pending_request.callback));
  }
  pending_requests_.clear();
}

void ConnectionTokenAttestation::FailPendingRequestsAndCallOnDisconnect(
    ErrorCode error_code) {
  attestation_state_ = AttestationState::kFailed;

  for (auto& pending_request : pending_requests_) {
    std::move(pending_request.callback).Run(base::unexpected(error_code));
  }
  pending_requests_.clear();

  if (on_disconnect_) {
    std::move(on_disconnect_).Run();
  }
}

}  // namespace private_ai

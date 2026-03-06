// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_token_attestation.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"

namespace private_ai {

namespace internal {

std::string Base64ToWebSafeBase64(std::string str) {
  std::replace(str.begin(), str.end(), '+', '-');
  std::replace(str.begin(), str.end(), '/', '_');
  while (!str.empty() && str.back() == '=') {
    str.pop_back();
  }
  return str;
}

}  // namespace internal

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
  if (attestation_state_ == AttestationState::kTokenSent ||
      attestation_state_ == AttestationState::kTokenSuccess) {
    inner_connection_->Send(
        std::move(request), timeout,
        base::BindOnce(&ConnectionTokenAttestation::OnInnerConnectionResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  if (attestation_state_ == AttestationState::kTokenFailed) {
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

  // The `quiche::BlindSignAuth` library returns the token and extensions
  // encoded in standard Base64. However, the Private AI server expects these
  // fields to be encoded in WebSafeBase64. We perform this conversion here.
  std::string token_str = internal::Base64ToWebSafeBase64(auth_token->token);
  std::string extensions_str =
      internal::Base64ToWebSafeBase64(auth_token->encoded_extensions);

  privacy::ppn::PrivacyPassTokenData token_data;
  token_data.set_token(token_str);
  token_data.set_encoded_extensions(extensions_str);

  proto::PrivateAiRequest request_proto;

  request_proto.set_feature_name(
      proto::FeatureName::FEATURE_NAME_CHROME_CLIENT_ATTESTATION);
  request_proto.mutable_anonymous_token_request()->set_anonymous_token(
      token_data.SerializeAsString());

  logger_->LogInfo(FROM_HERE, "Sending auth token");

  // Send the attestation request but do not wait for a response.
  // The server expects the first message in the stream to be an
  // AnonymousTokenRequest and does not send a response on success. It only
  // proceeds to read the next message (the actual data request) if
  // token verification is successful. By pipelining the token request and
  // the actual request immediately back-to-back, we eliminate a full
  // network round-trip time (RTT), and this is safe because the server
  // guarantees to process the token request before handling subsequent
  // messages on the same stream.
  inner_connection_->Send(std::move(request_proto), base::Seconds(60),
                          base::DoNothing());

  attestation_state_ = AttestationState::kTokenSent;

  auto pending_requests = std::move(pending_requests_);
  for (auto& pending_request : pending_requests) {
    inner_connection_->Send(
        std::move(pending_request.request), pending_request.timeout,
        base::BindOnce(&ConnectionTokenAttestation::OnInnerConnectionResponse,
                       weak_factory_.GetWeakPtr(),
                       std::move(pending_request.callback)));
  }
}

void ConnectionTokenAttestation::OnInnerConnectionResponse(
    OnRequestCallback original_callback,
    base::expected<proto::PrivateAiResponse, ErrorCode> result) {
  if (attestation_state_ == AttestationState::kTokenSent) {
    if (!result.has_value()) {
      // If *any* error occurs before we receive the first successful response
      // after sending the token, we assume it's an attestation failure caused
      // by an invalid token. The server closes the stream on invalid token,
      // which surfaces as an error here.
      logger_->LogError(
          FROM_HERE,
          base::StrCat({"Request failed with error code: ",
                        base::NumberToString(static_cast<int>(result.error())),
                        ", assuming token rejection."}));
      attestation_state_ = AttestationState::kTokenFailed;
      std::move(original_callback)
          .Run(base::unexpected(ErrorCode::kClientAttestationFailed));
      // The connection is now considered broken due to failed attestation.
      CallOnDisconnect(ErrorCode::kClientAttestationFailed);
      return;
    } else {
      // If we reach here with result.has_value() and we were in kTokenSent
      // state, it means this is the first successful response, so the token was
      // accepted.
      attestation_state_ = AttestationState::kTokenSuccess;
    }
  }

  std::move(original_callback).Run(std::move(result));
}

void ConnectionTokenAttestation::OnDestroy(ErrorCode error) {
  attestation_state_ = AttestationState::kTokenFailed;
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

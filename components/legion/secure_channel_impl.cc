// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/secure_channel_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "components/legion/attestation_handler.h"
#include "components/legion/legion_common.h"
#include "components/legion/oak_session.h"
#include "components/legion/transport.h"

namespace legion {

SecureChannelImpl::PendingRequest::PendingRequest(
    Request request,
    OnResponseReceivedCallback callback)
    : request(std::move(request)), callback(std::move(callback)) {}

SecureChannelImpl::PendingRequest::~PendingRequest() = default;

SecureChannelImpl::PendingRequest::PendingRequest(PendingRequest&&) = default;

SecureChannelImpl::PendingRequest& SecureChannelImpl::PendingRequest::operator=(
    PendingRequest&&) = default;

SecureChannelImpl::SecureChannelImpl(
    std::unique_ptr<Transport> transport,
    std::unique_ptr<OakSession> oak_session,
    std::unique_ptr<AttestationHandler> attestation_handler)
    : transport_(std::move(transport)),
      oak_session_(std::move(oak_session)),
      attestation_handler_(std::move(attestation_handler)) {
  CHECK(transport_);
  CHECK(oak_session_);
  CHECK(attestation_handler_);
}

SecureChannelImpl::~SecureChannelImpl() = default;

void SecureChannelImpl::Write(Request request,
                          OnResponseReceivedCallback callback) {
  pending_requests_.emplace_back(std::move(request), std::move(callback));

  switch (state_) {
    case State::kUninitialized:
      StartSessionEstablishment();
      break;
    case State::kEstablishingSession:
      // Request is queued and will be processed once the session is
      // established.
      break;
    case State::kEstablished:
      // The session is established. A new request is sent only if there is
      // no other request in flight.
      ProcessNextRequest();
      break;
    case State::kPermanentFailure:
      DLOG(ERROR) << "SecureChannel is in a permanent failure state.";
      FailAllPendingRequests(ResultCode::kError);
      break;
  }
}

void SecureChannelImpl::OnAttestationResponse(
    base::expected<Response, Transport::TransportError> response) {
  if (!response.has_value()) {
    DLOG(ERROR) << "Transport error during attestation: "
                << static_cast<int>(response.error());
    FailAllPendingRequests(ResultCode::kNetworkError);
    state_ = State::kPermanentFailure;
    return;
  }

  // Step 2: Verify Attestation Response
  if (!attestation_handler_->VerifyAttestationResponse(response.value())) {
    DLOG(ERROR) << "Attestation verification failed.";
    FailAllPendingRequests(ResultCode::kAttestationFailed);
    ResetState();
    return;
  }
  DVLOG(1) << "Attestation verified successfully.";

  // Step 3: Get and Send Handshake Request
  std::optional<Request> handshake_request =
      oak_session_->GetHandshakeMessage();
  if (!handshake_request.has_value()) {
    DLOG(ERROR) << "Failed to get handshake request.";
    FailAllPendingRequests(ResultCode::kHandshakeFailed);
    ResetState();
    return;
  }

  DVLOG(1) << "Sending handshake request.";
  transport_->Send(std::move(handshake_request.value()),
                   base::BindOnce(&SecureChannelImpl::OnHandshakeResponse,
                                  weak_factory_.GetWeakPtr()));
}

void SecureChannelImpl::OnHandshakeResponse(
    base::expected<Response, Transport::TransportError> response) {
  if (!response.has_value()) {
    DLOG(ERROR) << "Transport error during handshake: "
                << static_cast<int>(response.error());
    FailAllPendingRequests(ResultCode::kNetworkError);
    state_ = State::kPermanentFailure;
    return;
  }

  // Step 4: Process Handshake Response
  if (!oak_session_->ProcessHandshakeResponse(response.value())) {
    DLOG(ERROR) << "Failed to handle handshake response.";
    FailAllPendingRequests(ResultCode::kHandshakeFailed);
    ResetState();
    return;
  }
  DVLOG(1) << "Handshake response handled successfully.";

  state_ = State::kEstablished;
  ProcessNextRequest();
}

void SecureChannelImpl::OnEncryptedResponse(
    base::expected<Response, Transport::TransportError> response) {
  DCHECK(request_in_flight_);
  request_in_flight_ = false;

  if (!response.has_value()) {
    DLOG(ERROR) << "Transport error receiving encrypted response: "
                << static_cast<int>(response.error());
    FailAllPendingRequests(ResultCode::kNetworkError);
    state_ = State::kPermanentFailure;
    return;
  }

  // Step 6: Decrypt the response
  std::optional<Request> decrypted_response =
      oak_session_->Decrypt(response.value());
  if (!decrypted_response.has_value()) {
    DLOG(ERROR) << "Failed to decrypt response.";
    FailAllPendingRequests(ResultCode::kDecryptionFailed);
    ResetState();
    return;
  }
  DVLOG(1) << "Response decrypted successfully.";

  DCHECK(!pending_requests_.empty());
  std::move(pending_requests_.front().callback)
      .Run(ResultCode::kSuccess, std::move(decrypted_response));
  pending_requests_.pop_front();

  ProcessNextRequest();
}

void SecureChannelImpl::ResetState() {
  state_ = State::kUninitialized;
  request_in_flight_ = false;
}

void SecureChannelImpl::FailAllPendingRequests(ResultCode result_code) {
  for (auto& pending_request : pending_requests_) {
    std::move(pending_request.callback).Run(result_code, std::nullopt);
  }
  pending_requests_.clear();
}

void SecureChannelImpl::StartSessionEstablishment() {
  DCHECK_EQ(state_, State::kUninitialized);
  DCHECK(!pending_requests_.empty());

  // Step 1: Get and Send Attestation Request
  std::optional<Request> attestation_req =
      attestation_handler_->GetAttestationRequest();
  if (!attestation_req.has_value()) {
    DLOG(ERROR) << "Failed to get attestation request.";
    FailAllPendingRequests(ResultCode::kAttestationFailed);
    ResetState();
    return;
  }

  state_ = State::kEstablishingSession;
  DVLOG(1) << "Sending attestation request.";
  transport_->Send(std::move(attestation_req.value()),
                   base::BindOnce(&SecureChannelImpl::OnAttestationResponse,
                                  weak_factory_.GetWeakPtr()));
}

void SecureChannelImpl::ProcessNextRequest() {
  if (pending_requests_.empty() || request_in_flight_) {
    return;
  }

  DCHECK_EQ(state_, State::kEstablished);

  // Step 5: Encrypt and Send the original request
  std::optional<Response> encrypted_request =
      oak_session_->Encrypt(pending_requests_.front().request);
  if (!encrypted_request.has_value()) {
    DLOG(ERROR) << "Failed to encrypt request.";
    FailAllPendingRequests(ResultCode::kEncryptionFailed);
    ResetState();
    return;
  }
  DVLOG(1) << "Request encrypted successfully.";

  DVLOG(1) << "Sending encrypted request.";
  request_in_flight_ = true;
  transport_->Send(std::move(encrypted_request.value()),
                   base::BindOnce(&SecureChannelImpl::OnEncryptedResponse,
                                  weak_factory_.GetWeakPtr()));
}

}  // namespace legion

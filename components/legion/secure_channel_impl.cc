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
#include "third_party/oak/chromium/proto/session/session.pb.h"

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
    case State::kPerformingAttestation:
    case State::kPerformingHandshake:
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

void SecureChannelImpl::Send(
    const oak::session::v1::SessionRequest& session_request) {
  // TODO: OnResponseReceived should probably be a repeating callback set on
  // Transport to allow for parallel requests.
  transport_->Send(session_request,
                   base::BindOnce(&SecureChannelImpl::OnResponseReceived,
                                  weak_factory_.GetWeakPtr()));
}

void SecureChannelImpl::OnResponseReceived(
    base::expected<oak::session::v1::SessionResponse, Transport::TransportError>
        response) {
  if (!response.has_value()) {
    // TODO: derive result code from state_ and print state.
    DLOG(ERROR) << "Transport error: " << static_cast<int>(response.error());
    FailAllPendingRequests(ResultCode::kNetworkError);
    state_ = State::kPermanentFailure;
    return;
  }

  oak::session::v1::SessionResponse& session_response = response.value();
  if (session_response.has_attest_response()) {
    OnAttestationResponse(session_response.attest_response());
  } else if (session_response.has_handshake_response()) {
    OnHandshakeResponse(session_response.handshake_response());
  } else if (session_response.has_encrypted_message()) {
    OnEncryptedResponse(session_response.encrypted_message());
  } else {
    LOG(ERROR) << "Response does not contain any messages";
  }
}

void SecureChannelImpl::OnAttestationResponse(
    const oak::session::v1::AttestResponse& response) {
  DCHECK_EQ(state_, State::kPerformingAttestation);

  // Step 2: Verify Attestation Response
  if (!attestation_handler_->VerifyAttestationResponse(response)) {
    DLOG(ERROR) << "Attestation verification failed.";
    FailAllPendingRequests(ResultCode::kAttestationFailed);
    ResetState();
    return;
  }
  DVLOG(1) << "Attestation verified successfully.";

  state_ = SecureChannelImpl::State::kPerformingHandshake;
  // Step 3: Get and Send Handshake Request
  std::optional<oak::session::v1::HandshakeRequest> handshake_request =
      oak_session_->GetHandshakeMessage();
  if (!handshake_request.has_value()) {
    DLOG(ERROR) << "Failed to get handshake request.";
    FailAllPendingRequests(ResultCode::kHandshakeFailed);
    ResetState();
    return;
  }

  DVLOG(1) << "Sending handshake request.";
  oak::session::v1::SessionRequest request;
  *request.mutable_handshake_request() = std::move(handshake_request.value());
  Send(request);
}

void SecureChannelImpl::OnHandshakeResponse(
    const oak::session::v1::HandshakeResponse& response) {
  DCHECK_EQ(state_, State::kPerformingHandshake);

  // Step 4: Process Handshake Response
  if (!oak_session_->ProcessHandshakeResponse(response)) {
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
    const oak::session::v1::EncryptedMessage& response) {
  DCHECK(request_in_flight_);
  request_in_flight_ = false;

  // Step 6: Decrypt the response
  std::optional<Request> decrypted_response = oak_session_->Decrypt(response);
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
  std::optional<oak::session::v1::AttestRequest> attestation_req =
      attestation_handler_->GetAttestationRequest();
  if (!attestation_req.has_value()) {
    DLOG(ERROR) << "Failed to get attestation request.";
    FailAllPendingRequests(ResultCode::kAttestationFailed);
    ResetState();
    return;
  }

  state_ = State::kPerformingAttestation;
  DVLOG(1) << "Sending attestation request.";
  oak::session::v1::SessionRequest request;
  *request.mutable_attest_request() = std::move(attestation_req.value());
  Send(request);
}

void SecureChannelImpl::ProcessNextRequest() {
  DCHECK_EQ(state_, State::kEstablished);
  if (pending_requests_.empty() || request_in_flight_ ||
      state_ != State::kEstablished) {
    return;
  }

  // Step 5: Encrypt and Send the original request
  std::optional<oak::session::v1::EncryptedMessage> encrypted_request =
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
  oak::session::v1::SessionRequest request;
  *request.mutable_encrypted_message() = std::move(encrypted_request.value());
  Send(request);
}

}  // namespace legion

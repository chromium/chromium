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
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "components/legion/attestation_handler.h"
#include "components/legion/legion_common.h"
#include "components/legion/proto/legion.pb.h"
#include "components/legion/secure_session.h"
#include "components/legion/transport.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

SecureChannelImpl::SecureChannelImpl(
    std::unique_ptr<Transport> transport,
    std::unique_ptr<SecureSession> secure_session,
    std::unique_ptr<AttestationHandler> attestation_handler)
    : transport_(std::move(transport)),
      secure_session_(std::move(secure_session)),
      attestation_handler_(std::move(attestation_handler)) {
  CHECK(transport_);
  CHECK(secure_session_);
  CHECK(attestation_handler_);

  transport_->SetResponseCallback(base::BindRepeating(
      &SecureChannelImpl::OnResponseReceived, weak_factory_.GetWeakPtr()));
}

SecureChannelImpl::~SecureChannelImpl() = default;

void SecureChannelImpl::SetResponseCallback(ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  response_callback_ = std::move(callback);
}

bool SecureChannelImpl::Write(const Request& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kClosed) {
    DLOG(ERROR) << "SecureChannel is closed.";
    return false;
  }

  pending_requests_.emplace_back(request);

  switch (state_) {
    case State::kUninitialized:
      StartSessionEstablishment();
      break;
    case State::kPerformingAttestation:
    case State::kWaitingHandshakeMessage:
    case State::kPerformingHandshake:
      // Request is queued and will be processed once the session is
      // established.
      break;
    case State::kEstablished:
      // The session is established. A new request is sent only if there is
      // no other request in flight.
      ProcessPendingRequests();
      break;
    case State::kClosed:
      // This case should not be reached because of the check at the top.
      NOTREACHED();
  }
  return true;
}

void SecureChannelImpl::Send(
    const oak::session::v1::SessionRequest& session_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transport_->Send(session_request);
}

void SecureChannelImpl::OnResponseReceived(
    base::expected<oak::session::v1::SessionResponse, Transport::TransportError>
        response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!response.has_value()) {
    DLOG(ERROR) << "Transport error: " << static_cast<int>(response.error())
                << " in state: " << static_cast<int>(state_);

    ErrorCode error_code;
    switch (state_) {
      case State::kPerformingAttestation:
        error_code = ErrorCode::kAttestationFailed;
        break;
      case State::kPerformingHandshake:
        error_code = ErrorCode::kHandshakeFailed;
        break;
      case State::kEstablished:
        error_code = ErrorCode::kNetworkError;
        break;
      case State::kWaitingHandshakeMessage:
      case State::kUninitialized:
      case State::kClosed:
        // Transport error in these states is unexpected because no requests
        // should be in flight.
        NOTREACHED() << "Unexpected transport error in state: "
                     << static_cast<int>(state_);
    }

    FailAllRequestsAndClose(error_code);
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
    FailAllRequestsAndClose(ErrorCode::kNetworkError);
  }
}

void SecureChannelImpl::OnAttestationResponse(
    const oak::session::v1::AttestResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kPerformingAttestation);

  // Step 2: Verify Attestation Response
  if (!attestation_handler_->VerifyAttestationResponse(response)) {
    DLOG(ERROR) << "Attestation verification failed.";
    FailAllRequestsAndClose(ErrorCode::kAttestationFailed);
    return;
  }
  DVLOG(1) << "Attestation verified successfully.";

  state_ = SecureChannelImpl::State::kWaitingHandshakeMessage;

  // Step 3: Get and Send Handshake Request
  secure_session_->GetHandshakeMessage(base::BindOnce(
      &SecureChannelImpl::OnHandshakeMessageReady, weak_factory_.GetWeakPtr()));
}

void SecureChannelImpl::OnHandshakeMessageReady(
    oak::session::v1::HandshakeRequest handshake_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kWaitingHandshakeMessage);

  state_ = SecureChannelImpl::State::kPerformingHandshake;

  DVLOG(1) << "Sending handshake request.";
  oak::session::v1::SessionRequest request;
  *request.mutable_handshake_request() = std::move(handshake_request);
  Send(request);
}

void SecureChannelImpl::OnHandshakeResponse(
    const oak::session::v1::HandshakeResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kPerformingHandshake);

  // Step 4: Process Handshake Response
  if (!secure_session_->ProcessHandshakeResponse(response)) {
    DLOG(ERROR) << "Failed to handle handshake response.";
    FailAllRequestsAndClose(ErrorCode::kHandshakeFailed);
    return;
  }
  DVLOG(1) << "Handshake response handled successfully.";

  state_ = State::kEstablished;
  ProcessPendingRequests();
}

void SecureChannelImpl::OnEncryptedResponse(
    const oak::session::v1::EncryptedMessage& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Step 6: Decrypt the response
  std::optional<Request> decrypted_response =
      secure_session_->Decrypt(response);
  if (!decrypted_response.has_value()) {
    DLOG(ERROR) << "Failed to decrypt response.";
    FailAllRequestsAndClose(ErrorCode::kDecryptionFailed);
    return;
  }
  DVLOG(1) << "Response decrypted successfully.";

  CHECK(response_callback_);
  response_callback_.Run(base::ok(std::move(*decrypted_response)));

  ProcessPendingRequests();
}

void SecureChannelImpl::FailAllRequests(ErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_requests_.clear();
}

void SecureChannelImpl::StartSessionEstablishment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kUninitialized);
  DCHECK(!pending_requests_.empty());

  // Step 1: Get and Send Attestation Request
  std::optional<oak::session::v1::AttestRequest> attestation_req =
      attestation_handler_->GetAttestationRequest();
  if (!attestation_req.has_value()) {
    DLOG(ERROR) << "Failed to get attestation request.";
    FailAllRequestsAndClose(ErrorCode::kAttestationFailed);
    return;
  }

  state_ = State::kPerformingAttestation;
  DVLOG(1) << "Sending attestation request.";
  oak::session::v1::SessionRequest request;
  *request.mutable_attest_request() = std::move(attestation_req.value());
  Send(request);
}

void SecureChannelImpl::FailAllRequestsAndClose(ErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  FailAllRequests(error_code);
  state_ = State::kClosed;
  CHECK(response_callback_);
  response_callback_.Run(base::unexpected(error_code));
}

void SecureChannelImpl::ProcessPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kEstablished);
  if (state_ != State::kEstablished) {
    return;
  }

  while (!pending_requests_.empty()) {
    // Step 5: Encrypt and Send the original request
    std::optional<oak::session::v1::EncryptedMessage> encrypted_request =
        secure_session_->Encrypt(pending_requests_.front());

    if (!encrypted_request.has_value()) {
      DLOG(ERROR) << "Failed to encrypt request.";
      FailAllRequestsAndClose(ErrorCode::kEncryptionFailed);
      return;
    }
    DVLOG(1) << "Request encrypted successfully.";

    pending_requests_.pop_front();

    DVLOG(1) << "Sending encrypted request.";
    oak::session::v1::SessionRequest request;
    *request.mutable_encrypted_message() = std::move(encrypted_request.value());
    Send(request);
  }
}

}  // namespace legion

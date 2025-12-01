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
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
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

SecureChannelImpl::~SecureChannelImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordSessionDurationMetrics();
}

void SecureChannelImpl::SetResponseCallback(ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  response_callback_ = std::move(callback);
}

void SecureChannelImpl::EstablishChannel(EstablishChannelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_) {
    case State::kUninitialized:
      pending_establishment_callbacks_.push_back(std::move(callback));
      StartSessionEstablishment();
      return;
    case State::kPerformingAttestation:
    case State::kWaitingHandshakeMessage:
    case State::kPerformingHandshake:
    case State::kVerifyingHandshake:
      pending_establishment_callbacks_.push_back(std::move(callback));
      return;
    case State::kEstablished:
      std::move(callback).Run(base::ok());
      return;
    case State::kClosed:
      DLOG(ERROR) << "SecureChannel is closed.";
      std::move(callback).Run(base::unexpected(ErrorCode::kError));
      return;
  }
}

bool SecureChannelImpl::Write(const Request& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kClosed) {
    DLOG(ERROR) << "SecureChannel is closed.";
    return false;
  }

  requests_in_session_count_++;
  pending_encryption_requests_.push_back(request);

  switch (state_) {
    case State::kUninitialized:
      StartSessionEstablishment();
      break;
    case State::kPerformingAttestation:
    case State::kWaitingHandshakeMessage:
    case State::kPerformingHandshake:
    case State::kVerifyingHandshake:
      // Request is queued and will be processed once the session is
      // established.
      break;
    case State::kEstablished:
      // The session is established. A new request is sent only if there is
      // no other request in flight.
      ProcessPendingEncryptionRequests();
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
      case State::kVerifyingHandshake:
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
    base::UmaHistogramMediumTimes(
        "Legion.SecureChannel.SendAttestationRequestLatency.Error",
        base::TimeTicks::Now() -
            state_entry_times_[State::kPerformingAttestation]);
    FailAllRequestsAndClose(ErrorCode::kAttestationFailed);
    return;
  }
  DVLOG(1) << "Attestation verified successfully.";
  base::UmaHistogramMediumTimes(
      "Legion.SecureChannel.SendAttestationRequestLatency.Success",
      base::TimeTicks::Now() -
          state_entry_times_[State::kPerformingAttestation]);

  state_ = SecureChannelImpl::State::kWaitingHandshakeMessage;
  state_entry_times_[state_] = base::TimeTicks::Now();

  // Step 3: Get and Send Handshake Request
  secure_session_->GetHandshakeMessage(base::BindOnce(
      &SecureChannelImpl::OnHandshakeMessageReady, weak_factory_.GetWeakPtr()));
}

void SecureChannelImpl::OnHandshakeMessageReady(
    oak::session::v1::HandshakeRequest handshake_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kWaitingHandshakeMessage);

  base::UmaHistogramMediumTimes(
      "Legion.SecureChannel.GetHandshakeMessageLatency.Success",
      base::TimeTicks::Now() -
          state_entry_times_[State::kWaitingHandshakeMessage]);

  state_ = SecureChannelImpl::State::kPerformingHandshake;
  state_entry_times_[state_] = base::TimeTicks::Now();

  DVLOG(1) << "Sending handshake request.";
  oak::session::v1::SessionRequest request;
  *request.mutable_handshake_request() = std::move(handshake_request);
  Send(request);
}

void SecureChannelImpl::RecordSessionDurationMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kEstablished) {
    base::UmaHistogramMediumTimes(
        "Legion.SecureChannel.SessionDuration",
        base::TimeTicks::Now() - state_entry_times_[State::kEstablished]);
    base::UmaHistogramCounts1000("Legion.SecureChannel.RequestsPerSession",
                                 requests_in_session_count_);
  }
}

void SecureChannelImpl::OnHandshakeResponse(
    const oak::session::v1::HandshakeResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kPerformingHandshake);

  state_ = State::kVerifyingHandshake;

  // Step 4: Process Handshake Response
  secure_session_->ProcessHandshakeResponse(
      response, base::BindOnce(&SecureChannelImpl::OnHandshakeVerification,
                               weak_factory_.GetWeakPtr()));
}

void SecureChannelImpl::OnHandshakeVerification(bool handshake_verified) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kVerifyingHandshake);

  if (!handshake_verified) {
    DLOG(ERROR) << "Failed to handle handshake response.";
    base::UmaHistogramMediumTimes(
        "Legion.SecureChannel.SendHandshakeRequestLatency.Error",
        base::TimeTicks::Now() -
            state_entry_times_[State::kPerformingHandshake]);
    FailAllRequestsAndClose(ErrorCode::kHandshakeFailed);
    return;
  }
  DVLOG(1) << "Handshake response handled successfully.";

  base::UmaHistogramMediumTimes(
      "Legion.SecureChannel.SendHandshakeRequestLatency.Success",
      base::TimeTicks::Now() - state_entry_times_[State::kPerformingHandshake]);

  state_ = State::kEstablished;
  state_entry_times_[state_] = base::TimeTicks::Now();

  auto callbacks = std::move(pending_establishment_callbacks_);
  pending_establishment_callbacks_.clear();
  for (auto& cb : callbacks) {
    std::move(cb).Run(base::ok());
  }

  ProcessPendingEncryptionRequests();
}

void SecureChannelImpl::OnEncryptedResponse(
    const oak::session::v1::EncryptedMessage& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Step 6: Decrypt the response
  secure_session_->Decrypt(
      response, base::BindOnce(&SecureChannelImpl::OnDecryptedResponse,
                               weak_factory_.GetWeakPtr()));
}

void SecureChannelImpl::OnDecryptedResponse(
    std::optional<Request> decrypted_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!decrypted_response.has_value()) {
    DLOG(ERROR) << "Failed to decrypt response.";
    FailAllRequestsAndClose(ErrorCode::kDecryptionFailed);
    return;
  }
  DVLOG(1) << "Response decrypted successfully.";

  CHECK(response_callback_);
  response_callback_.Run(base::ok(std::move(*decrypted_response)));

  ProcessPendingEncryptionRequests();
}

void SecureChannelImpl::StartSessionEstablishment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kUninitialized);
  DCHECK(!pending_encryption_requests_.empty() ||
         !pending_establishment_callbacks_.empty());

  // Step 1: Get and Send Attestation Request
  auto get_attestation_start_time = base::TimeTicks::Now();
  std::optional<oak::session::v1::AttestRequest> attestation_req =
      attestation_handler_->GetAttestationRequest();
  if (!attestation_req.has_value()) {
    DLOG(ERROR) << "Failed to get attestation request.";
    base::UmaHistogramMediumTimes(
        "Legion.SecureChannel.GetAttestationRequestLatency.Error",
        base::TimeTicks::Now() - get_attestation_start_time);
    FailAllRequestsAndClose(ErrorCode::kAttestationFailed);
    return;
  }
  base::UmaHistogramMediumTimes(
      "Legion.SecureChannel.GetAttestationRequestLatency.Success",
      base::TimeTicks::Now() - get_attestation_start_time);

  state_ = State::kPerformingAttestation;
  state_entry_times_[state_] = base::TimeTicks::Now();
  DVLOG(1) << "Sending attestation request.";
  oak::session::v1::SessionRequest request;
  *request.mutable_attest_request() = std::move(attestation_req.value());
  Send(request);
}

void SecureChannelImpl::FailAllRequestsAndClose(ErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kClosed) {
    return;
  }

  RecordSessionDurationMetrics();

  state_ = State::kClosed;

  auto establishment_callbacks = std::move(pending_establishment_callbacks_);
  pending_establishment_callbacks_.clear();
  for (auto& cb : establishment_callbacks) {
    std::move(cb).Run(base::unexpected(error_code));
  }

  pending_encryption_requests_.clear();

  if (response_callback_) {
    response_callback_.Run(base::unexpected(error_code));
  }
}

void SecureChannelImpl::ProcessPendingEncryptionRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kEstablished);
  if (state_ != State::kEstablished) {
    return;
  }

  while (!pending_encryption_requests_.empty()) {
    // Step 5: Encrypt and Send the original request
    secure_session_->Encrypt(
        pending_encryption_requests_.front(),
        base::BindOnce(&SecureChannelImpl::OnRequestEncrypted,
                       weak_factory_.GetWeakPtr()));
    pending_encryption_requests_.pop_front();
  }
}

void SecureChannelImpl::OnRequestEncrypted(
    std::optional<oak::session::v1::EncryptedMessage> encrypted_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kEstablished) {
    return;
  }

  if (!encrypted_request.has_value()) {
    DLOG(ERROR) << "Failed to encrypt request.";
    FailAllRequestsAndClose(ErrorCode::kEncryptionFailed);
    return;
  }

  DVLOG(1) << "Sending encrypted request.";
  oak::session::v1::SessionRequest request;
  *request.mutable_encrypted_message() = std::move(encrypted_request.value());
  Send(request);
}

}  // namespace legion

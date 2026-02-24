// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/secure_channel_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/attestation/handler.h"
#include "components/private_ai/attestation/handler_impl.h"
#include "components/private_ai/private_ai_common.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "components/private_ai/proto_utils/attestation_evidence_utils.h"
#include "components/private_ai/secure_session.h"
#include "components/private_ai/secure_session_async_impl.h"
#include "components/private_ai/transport.h"
#include "components/private_ai/websocket_client.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "url/gurl.h"

namespace private_ai {

SecureChannelImpl::FactoryImpl::FactoryImpl(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    PrivateAiLogger* logger)
    : url_(url), network_context_(network_context), logger_(logger) {}

SecureChannelImpl::FactoryImpl::~FactoryImpl() = default;

std::unique_ptr<SecureChannel> SecureChannelImpl::FactoryImpl::Create(
    ResponseCallback callback) {
  auto transport =
      std::make_unique<WebSocketClient>(url_, network_context_, logger_);
  auto secure_session = std::make_unique<SecureSessionAsyncImpl>();
  auto attestation_handler = std::make_unique<AttestationHandlerImpl>();

  return std::make_unique<SecureChannelImpl>(
      std::move(callback), std::move(transport), std::move(secure_session),
      std::move(attestation_handler), logger_);
}

SecureChannelImpl::SecureChannelImpl(
    ResponseCallback callback,
    std::unique_ptr<Transport> transport,
    std::unique_ptr<SecureSession> secure_session,
    std::unique_ptr<AttestationHandler> attestation_handler,
    PrivateAiLogger* logger)
    : transport_(std::move(transport)),
      secure_session_(std::move(secure_session)),
      attestation_handler_(std::move(attestation_handler)),
      logger_(logger),
      response_callback_(std::move(callback)) {
  CHECK(transport_);
  CHECK(secure_session_);
  CHECK(attestation_handler_);
  CHECK(logger_);

  transport_->SetResponseCallback(base::BindRepeating(
      &SecureChannelImpl::OnResponseReceived, weak_factory_.GetWeakPtr()));

  StartSessionEstablishment();
}

SecureChannelImpl::~SecureChannelImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordSessionDurationMetrics();
}

bool SecureChannelImpl::Write(const Request& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (state_) {
    case State::kPerformingAttestation:
    case State::kWaitingHandshakeMessage:
    case State::kPerformingHandshake:
    case State::kVerifyingHandshake:
      // Request is queued and will be processed once the session is
      // established.
      AddRequestToPendingEncryptionQueue(request);
      return true;
    case State::kEstablished:
      // The session is established. A new request is sent only if there is
      // no other request in flight.
      AddRequestToPendingEncryptionQueue(request);
      ProcessPendingEncryptionRequests();
      return true;
    case State::kClosed:
      logger_->LogError(FROM_HERE, "SecureChannel is closed.");
      return false;
  }
}

void SecureChannelImpl::AddRequestToPendingEncryptionQueue(
    const Request& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  requests_in_session_count_++;
  pending_encryption_requests_.push_back(request);
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
    logger_->LogError(FROM_HERE,
                      base::StringPrintf("Transport error: %d in state: %d",
                                         static_cast<int>(response.error()),
                                         static_cast<int>(state_)));

    ErrorCode error_code = ErrorCode::kError;
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
      case State::kClosed:
        // Transport error in these states is unexpected because no requests
        // should be in flight.
        //
        // Nevertheless, we do not crash here as this branch could be triggered
        // by misbehaving server.
        logger_->LogError(
            FROM_HERE,
            base::StringPrintf("Unexpected transport error in state: %d",
                               static_cast<int>(state_)));
        break;
    }

    FailAllRequestsAndClose(error_code);
    return;
  }

  CHECK(response.has_value());

  const oak::session::v1::SessionResponse& session_response = response.value();

  switch (state_) {
    case State::kPerformingAttestation:
      OnAttestationResponse(session_response);
      break;
    case State::kPerformingHandshake:
      OnHandshakeResponse(session_response);
      break;
    case State::kEstablished:
      OnEncryptedResponse(session_response);
      break;
    case State::kWaitingHandshakeMessage:
    case State::kVerifyingHandshake:
    case State::kClosed:
      logger_->LogError(
          FROM_HERE,
          base::StringPrintf("Received unexpected response in state: %d",
                             static_cast<int>(state_)));
      break;
  }
}

void SecureChannelImpl::OnAttestationResponse(
    const oak::session::v1::SessionResponse& session_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kPerformingAttestation);

  if (!session_response.has_attest_response()) {
    logger_->LogError(FROM_HERE,
                      "Response proto does not have attestation message.");
    base::UmaHistogramMediumTimes(
        "PrivateAi.SecureChannel.SendAttestationRequestLatency.Error",
        base::TimeTicks::Now() -
            state_entry_times_[State::kPerformingAttestation]);
    FailAllRequestsAndClose(ErrorCode::kAttestationFailed);
    return;
  }

  const oak::session::v1::AttestResponse& response =
      session_response.attest_response();

  // Step 2: Verify Attestation Response
  std::optional<AttestationEvidence> attestation_evidence =
      ConvertToAttestationEvidence(response);
  if (!attestation_evidence) {
    logger_->LogError(FROM_HERE, "Attestation evidence conversion failed.");
    base::UmaHistogramMediumTimes(
        "PrivateAi.SecureChannel.SendAttestationRequestLatency.Error",
        base::TimeTicks::Now() -
            state_entry_times_[State::kPerformingAttestation]);
    FailAllRequestsAndClose(ErrorCode::kAttestationFailed);
    return;
  }

  if (!attestation_handler_->VerifyAttestationResponse(*attestation_evidence)) {
    logger_->LogError(FROM_HERE, "Attestation verification failed.");
    base::UmaHistogramMediumTimes(
        "PrivateAi.SecureChannel.SendAttestationRequestLatency.Error",
        base::TimeTicks::Now() -
            state_entry_times_[State::kPerformingAttestation]);
    FailAllRequestsAndClose(ErrorCode::kAttestationFailed);
    return;
  }
  logger_->LogInfo(FROM_HERE, "Attestation verified successfully.");
  base::UmaHistogramMediumTimes(
      "PrivateAi.SecureChannel.SendAttestationRequestLatency.Success",
      base::TimeTicks::Now() -
          state_entry_times_[State::kPerformingAttestation]);

  state_ = SecureChannelImpl::State::kWaitingHandshakeMessage;
  state_entry_times_[state_] = base::TimeTicks::Now();

  // Step 3: Get and Send Handshake Request
  secure_session_->GetHandshakeMessage(base::BindOnce(
      &SecureChannelImpl::OnHandshakeMessageReady, weak_factory_.GetWeakPtr()));
}

void SecureChannelImpl::OnHandshakeMessageReady(
    std::optional<oak::session::v1::HandshakeRequest> handshake_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kWaitingHandshakeMessage);

  if (!handshake_request.has_value()) {
    logger_->LogError(FROM_HERE, "Failed to generate handshake request.");
    base::UmaHistogramMediumTimes(
        "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Error",
        base::TimeTicks::Now() -
            state_entry_times_[State::kPerformingHandshake]);
    FailAllRequestsAndClose(ErrorCode::kHandshakeFailed);
    return;
  }

  base::UmaHistogramMediumTimes(
      "PrivateAi.SecureChannel.GetHandshakeMessageLatency.Success",
      base::TimeTicks::Now() -
          state_entry_times_[State::kWaitingHandshakeMessage]);

  state_ = SecureChannelImpl::State::kPerformingHandshake;
  state_entry_times_[state_] = base::TimeTicks::Now();

  DVLOG(1) << "Sending handshake request.";
  oak::session::v1::SessionRequest request;
  *request.mutable_handshake_request() = std::move(handshake_request.value());
  Send(request);
}

void SecureChannelImpl::RecordSessionDurationMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kEstablished) {
    base::UmaHistogramMediumTimes(
        "PrivateAi.SecureChannel.SessionDuration",
        base::TimeTicks::Now() - state_entry_times_[State::kEstablished]);
    base::UmaHistogramCounts1000("PrivateAi.SecureChannel.RequestsPerSession",
                                 requests_in_session_count_);
  }
}

void SecureChannelImpl::OnHandshakeResponse(
    const oak::session::v1::SessionResponse& session_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kPerformingHandshake);

  if (!session_response.has_handshake_response()) {
    logger_->LogError(FROM_HERE,
                      "Response proto does not have handshake message.");
    base::UmaHistogramMediumTimes(
        "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Error",
        base::TimeTicks::Now() -
            state_entry_times_[State::kPerformingHandshake]);
    FailAllRequestsAndClose(ErrorCode::kHandshakeFailed);
    return;
  }

  const oak::session::v1::HandshakeResponse& response =
      session_response.handshake_response();

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
    logger_->LogError(FROM_HERE, "Failed to handle handshake response.");
    base::UmaHistogramMediumTimes(
        "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Error",
        base::TimeTicks::Now() -
            state_entry_times_[State::kPerformingHandshake]);
    FailAllRequestsAndClose(ErrorCode::kHandshakeFailed);
    return;
  }
  logger_->LogInfo(FROM_HERE, "Session established.");

  base::UmaHistogramMediumTimes(
      "PrivateAi.SecureChannel.SendHandshakeRequestLatency.Success",
      base::TimeTicks::Now() - state_entry_times_[State::kPerformingHandshake]);

  state_ = State::kEstablished;
  state_entry_times_[state_] = base::TimeTicks::Now();

  ProcessPendingEncryptionRequests();
}

void SecureChannelImpl::OnEncryptedResponse(
    const oak::session::v1::SessionResponse& session_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kEstablished);

  if (!session_response.has_encrypted_message()) {
    logger_->LogError(FROM_HERE,
                      "Response proto does not have encrypted message.");
    FailAllRequestsAndClose(ErrorCode::kDecryptionFailed);
    return;
  }

  const oak::session::v1::EncryptedMessage& response =
      session_response.encrypted_message();

  // Step 6: Decrypt the response
  secure_session_->Decrypt(
      response, base::BindOnce(&SecureChannelImpl::OnDecryptedResponse,
                               weak_factory_.GetWeakPtr()));
}

void SecureChannelImpl::OnDecryptedResponse(
    const std::optional<Request>& decrypted_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!decrypted_response.has_value()) {
    logger_->LogError(FROM_HERE, "Failed to decrypt response.");
    FailAllRequestsAndClose(ErrorCode::kDecryptionFailed);
    return;
  }
  DVLOG(1) << "Response decrypted successfully.";

  CHECK(response_callback_);
  response_callback_.Run(base::ok(*decrypted_response));

  ProcessPendingEncryptionRequests();
}

void SecureChannelImpl::StartSessionEstablishment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(state_, State::kPerformingAttestation);

  logger_->LogInfo(FROM_HERE, "Starting session.");

  // Step 1: Get and Send Attestation Request
  auto get_attestation_start_time = base::TimeTicks::Now();
  std::optional<oak::session::v1::AttestRequest> attestation_req =
      attestation_handler_->GetAttestationRequest();
  if (!attestation_req.has_value()) {
    logger_->LogError(FROM_HERE, "Failed to get attestation request.");
    base::UmaHistogramMediumTimes(
        "PrivateAi.SecureChannel.GetAttestationRequestLatency.Error",
        base::TimeTicks::Now() - get_attestation_start_time);
    FailAllRequestsAndClose(ErrorCode::kAttestationFailed);
    return;
  }
  base::UmaHistogramMediumTimes(
      "PrivateAi.SecureChannel.GetAttestationRequestLatency.Success",
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

  logger_->LogError(
      FROM_HERE, base::StringPrintf("SecureChannel closed with error code: %d",
                                    static_cast<int>(error_code)));

  RecordSessionDurationMetrics();

  state_ = State::kClosed;

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
    logger_->LogError(FROM_HERE, "Failed to encrypt request.");
    FailAllRequestsAndClose(ErrorCode::kEncryptionFailed);
    return;
  }

  DVLOG(1) << "Sending encrypted request.";
  oak::session::v1::SessionRequest request;
  *request.mutable_encrypted_message() = std::move(encrypted_request.value());
  Send(request);
}

}  // namespace private_ai

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/tls_secure_channel_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"

namespace private_ai {

TlsSecureChannelImpl::FactoryImpl::FactoryImpl(
    const GURL& url,
    network::NetworkContextGetter network_context_getter,
    PrivateAiLogger* logger,
    PrivateAiOakSessionDriver* oak_session_driver,
    TlsCertificateRepository* cert_repository,
    std::string expected_server_name,
    std::optional<bool> has_chrome_proxy)
    : url_(url),
      network_context_getter_(std::move(network_context_getter)),
      logger_(logger),
      oak_session_driver_(oak_session_driver),
      cert_repository_(cert_repository),
      expected_server_name_(std::move(expected_server_name)),
      has_chrome_proxy_(has_chrome_proxy) {
  CHECK(cert_repository_);
  CHECK(oak_session_driver_);
  CHECK(logger_);
}

TlsSecureChannelImpl::FactoryImpl::~FactoryImpl() = default;

std::unique_ptr<SecureChannel> TlsSecureChannelImpl::FactoryImpl::Create(
    base::OnceClosure on_established,
    ResponseCallback callback) {
  auto transport_factory = base::BindRepeating(
      [](const GURL& url, network::NetworkContextGetter getter,
         PrivateAiLogger* logger, std::optional<bool> has_chrome_proxy) {
        return std::make_unique<TlsWebSocketClient>(url, getter, logger,
                                                    has_chrome_proxy);
      },
      url_, network_context_getter_, logger_, has_chrome_proxy_);

  auto session_factory =
      base::BindRepeating(&PrivateAiOakSessionDriver::BindOakTlsSessionService,
                          base::Unretained(oak_session_driver_));

  return std::make_unique<TlsSecureChannelImpl>(
      std::move(on_established), std::move(callback),
      std::move(transport_factory), std::move(session_factory),
      cert_repository_, logger_, expected_server_name_);
}

TlsSecureChannelImpl::TlsSecureChannelImpl(
    base::OnceClosure on_established,
    ResponseCallback callback,
    TransportFactory transport_factory,
    OakSessionFactory oak_session_factory,
    TlsCertificateRepository* cert_repository,
    PrivateAiLogger* logger,
    std::string expected_server_name)
    : response_callback_(std::move(callback)),
      on_established_(std::move(on_established)),
      transport_factory_(std::move(transport_factory)),
      oak_session_factory_(std::move(oak_session_factory)),
      cert_repository_(cert_repository),
      logger_(logger),
      expected_server_name_(std::move(expected_server_name)) {
  CHECK(transport_factory_);
  CHECK(oak_session_factory_);
  CHECK(cert_repository_);
  CHECK(logger_);

  start_time_ = base::TimeTicks::Now();
  StartSessionEstablishment(/*force_refresh=*/false);
}

TlsSecureChannelImpl::~TlsSecureChannelImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordSessionDurationMetrics();
}

bool TlsSecureChannelImpl::Write(const Request& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kClosed) {
    logger_->LogError(FROM_HERE, "TlsSecureChannel is closed.");
    return false;
  }

  requests_in_session_count_++;
  if (state_ == State::kEstablished) {
    EncryptRequest(request);
    return true;
  }

  pending_requests_.push_back(request);
  return true;
}

void TlsSecureChannelImpl::StartSessionEstablishment(bool force_refresh) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kFetchingCertificates;
  handshake_frame_in_flight_ = false;
  pending_incoming_frames_.clear();
  transport_.reset();
  oak_session_.reset();

  auto cert_cb = base::BindOnce(&TlsSecureChannelImpl::OnCertificatesFetched,
                                weak_factory_.GetWeakPtr());
  if (force_refresh) {
    cert_repository_->ForceRefresh(std::move(cert_cb));
  } else {
    cert_repository_->GetCertificates(std::move(cert_cb));
  }
}

void TlsSecureChannelImpl::OnCertificatesFetched(
    std::optional<TlsCertificates> certs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kFetchingCertificates) {
    return;
  }

  if (!certs.has_value()) {
    logger_->LogError(FROM_HERE, "Failed to load/fetch TCA root certificate.");
    FailAllRequestsAndClose(StatusCode::kAttestationFailed);
    return;
  }

  state_ = State::kStartingHandshake;

  transport_ = transport_factory_.Run();
  oak_session_ = oak_session_factory_.Run();
  CHECK(transport_);
  CHECK(oak_session_);

  oak_session_.set_disconnect_handler(
      base::BindOnce(&TlsSecureChannelImpl::OnOakSessionDisconnected,
                     weak_factory_.GetWeakPtr()));

  transport_->SetFrameCallback(
      base::BindRepeating(&TlsSecureChannelImpl::OnIncomingFrameReceived,
                          weak_factory_.GetWeakPtr()));

  oak_session_->StartHandshake(
      expected_server_name_, certs->tca_root_pem,
      base::BindOnce(&TlsSecureChannelImpl::OnStartTlsHandshakeDone,
                     weak_factory_.GetWeakPtr()));
}

void TlsSecureChannelImpl::OnStartTlsHandshakeDone(
    base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kStartingHandshake) {
    return;
  }

  if (!result.has_value()) {
    TlsHandshakeStatus status =
        result.error() == mojom::TlsHandshakeError::kCertVerificationFailed
            ? TlsHandshakeStatus::kCertVerificationFailed
            : TlsHandshakeStatus::kProtocolError;
    base::UmaHistogramEnumeration("PrivateAi.Tls.HandshakeStatus", status);
    logger_->LogError(FROM_HERE, "Failed to start TLS handshake.");
    FailAllRequestsAndClose(StatusCode::kHandshakeFailed);
    return;
  }

  state_ = State::kPerformingHandshake;

  if (!result.value()->outgoing_frame.empty()) {
    transport_->SendFrame(result.value()->outgoing_frame);
  }
}

void TlsSecureChannelImpl::OnIncomingFrameReceived(
    base::expected<std::vector<uint8_t>, TlsWebSocketClient::Error> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kClosed) {
    return;
  }

  if (!frame.has_value()) {
    logger_->LogError(FROM_HERE,
                      "WebSocket transport error during TLS session.");
    if (frame.error() == TlsWebSocketClient::Error::kConnectionFailed) {
      FailAllRequestsAndClose(StatusCode::kNetworkError);
      return;
    }

    switch (state_) {
      case State::kPerformingHandshake:
      case State::kStartingHandshake:
        FailAllRequestsAndClose(StatusCode::kHandshakeFailed);
        return;
      case State::kEstablished:
        FailAllRequestsAndClose(frame.error() ==
                                        TlsWebSocketClient::Error::kSocketClosed
                                    ? StatusCode::kConnectionClosedByServer
                                    : StatusCode::kNetworkError);
        return;
      case State::kFetchingCertificates:
      case State::kClosed:
        FailAllRequestsAndClose(StatusCode::kUnexpectedTransportError);
        return;
    }
  }

  switch (state_) {
    case State::kPerformingHandshake:
      if (handshake_frame_in_flight_) {
        pending_incoming_frames_.push_back(std::move(*frame));
        return;
      }
      handshake_frame_in_flight_ = true;
      oak_session_->ProcessHandshakeFrame(
          *frame,
          base::BindOnce(&TlsSecureChannelImpl::OnProcessHandshakeFrameDone,
                         weak_factory_.GetWeakPtr()));
      return;

    case State::kEstablished:
      if (frame->empty()) {
        return;
      }
      oak_session_->Decrypt(
          *frame, base::BindOnce(&TlsSecureChannelImpl::OnResponseDecrypted,
                                 weak_factory_.GetWeakPtr()));
      return;

    case State::kFetchingCertificates:
    case State::kStartingHandshake:
      logger_->LogError(FROM_HERE,
                        "Unexpected frame received in non-ready state.");
      return;

    case State::kClosed:
      return;
  }
}

void TlsSecureChannelImpl::OnProcessHandshakeFrameDone(
    base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kPerformingHandshake) {
    return;
  }
  handshake_frame_in_flight_ = false;

  if (!result.has_value()) {
    TlsHandshakeStatus status =
        result.error() == mojom::TlsHandshakeError::kCertVerificationFailed
            ? TlsHandshakeStatus::kCertVerificationFailed
            : TlsHandshakeStatus::kProtocolError;
    base::UmaHistogramEnumeration("PrivateAi.Tls.HandshakeStatus", status);

    if (result.error() == mojom::TlsHandshakeError::kCertVerificationFailed) {
      if (!retried_on_cert_error_) {
        retried_on_cert_error_ = true;
        logger_->LogInfo(
            FROM_HERE,
            "Cert mismatch on handshake; refreshing certs and retrying.");
        StartSessionEstablishment(/*force_refresh=*/true);
        return;
      }
      logger_->LogError(FROM_HERE,
                        "Cert verification failed after cache refresh.");
      FailAllRequestsAndClose(StatusCode::kAttestationFailed);
      return;
    }

    logger_->LogError(FROM_HERE, "TLS handshake protocol error.");
    FailAllRequestsAndClose(StatusCode::kHandshakeFailed);
    return;
  }

  mojom::TlsHandshakePtr handshake = std::move(result.value());

  if (handshake->is_ready) {
    base::UmaHistogramEnumeration("PrivateAi.Tls.HandshakeStatus",
                                  TlsHandshakeStatus::kSuccess);
  }

  if (!handshake->outgoing_frame.empty()) {
    transport_->SendFrame(handshake->outgoing_frame);
  }

  if (handshake->is_ready) {
    state_ = State::kEstablished;
    established_time_ = base::TimeTicks::Now();
    logger_->LogInfo(FROM_HERE, "TLS 1.3 session established successfully.");
    base::UmaHistogramMediumTimes("PrivateAi.Tls.HandshakeLatency.Success",
                                  base::TimeTicks::Now() - start_time_);
    ProcessPendingRequests();
    if (on_established_) {
      std::move(on_established_).Run();
    }
  }

  while (!pending_incoming_frames_.empty() && state_ != State::kClosed &&
         !handshake_frame_in_flight_) {
    std::vector<uint8_t> next_frame =
        std::move(pending_incoming_frames_.front());
    pending_incoming_frames_.pop_front();
    OnIncomingFrameReceived(base::ok(std::move(next_frame)));
  }
}

void TlsSecureChannelImpl::OnOakSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_->LogError(FROM_HERE, "OakSession Mojo pipe disconnected.");
  FailAllRequestsAndClose(state_ == State::kEstablished
                              ? StatusCode::kError
                              : StatusCode::kHandshakeFailed);
}

void TlsSecureChannelImpl::ProcessPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kEstablished) {
    return;
  }

  std::vector<Request> requests = std::move(pending_requests_);
  for (const auto& request : requests) {
    EncryptRequest(request);
  }
}

void TlsSecureChannelImpl::EncryptRequest(const Request& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  oak_session_->Encrypt(
      request, base::BindOnce(&TlsSecureChannelImpl::OnRequestEncrypted,
                              weak_factory_.GetWeakPtr()));
}

void TlsSecureChannelImpl::OnRequestEncrypted(
    const std::optional<std::vector<uint8_t>>& encrypted_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kEstablished) {
    return;
  }

  if (!encrypted_data.has_value()) {
    logger_->LogError(FROM_HERE, "Failed to encrypt application request.");
    FailAllRequestsAndClose(StatusCode::kEncryptionFailed);
    return;
  }

  transport_->SendFrame(*encrypted_data);
}

void TlsSecureChannelImpl::OnResponseDecrypted(
    const std::optional<std::vector<uint8_t>>& decrypted_data,
    bool peer_closed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kEstablished) {
    return;
  }

  if (peer_closed) {
    FailAllRequestsAndClose(StatusCode::kConnectionClosedByServer);
    return;
  }

  if (!decrypted_data.has_value()) {
    logger_->LogError(FROM_HERE, "Failed to decrypt application response.");
    FailAllRequestsAndClose(StatusCode::kDecryptionFailed);
    return;
  }

  if (decrypted_data->empty()) {
    return;
  }

  if (response_callback_) {
    response_callback_.Run(base::ok(*decrypted_data));
  }
}

void TlsSecureChannelImpl::RecordSessionDurationMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kEstablished && established_time_.has_value()) {
    base::UmaHistogramLongTimes("PrivateAi.SecureChannel.SessionDuration2",
                                base::TimeTicks::Now() - *established_time_);
    base::UmaHistogramCounts1000("PrivateAi.SecureChannel.RequestsPerSession",
                                 requests_in_session_count_);
  }
}

void TlsSecureChannelImpl::FailAllRequestsAndClose(StatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kClosed) {
    return;
  }

  if (state_ != State::kEstablished && !start_time_.is_null()) {
    base::UmaHistogramMediumTimes("PrivateAi.Tls.HandshakeLatency.Failure",
                                  base::TimeTicks::Now() - start_time_);
  }

  RecordSessionDurationMetrics();

  state_ = State::kClosed;
  handshake_frame_in_flight_ = false;
  pending_incoming_frames_.clear();
  pending_requests_.clear();
  transport_.reset();
  oak_session_.reset();

  if (response_callback_) {
    response_callback_.Run(base::unexpected(status));
  }
}

}  // namespace private_ai

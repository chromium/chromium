// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TLS_SECURE_CHANNEL_IMPL_H_
#define COMPONENTS_PRIVATE_AI_TLS_SECURE_CHANNEL_IMPL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/mojom/oak_session.mojom.h"
#include "components/private_ai/private_ai_common.h"
#include "components/private_ai/private_ai_oak_session_driver.h"
#include "components/private_ai/secure_channel.h"
#include "components/private_ai/status_code.h"
#include "components/private_ai/tls_certificate_repository.h"
#include "components/private_ai/tls_websocket_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "url/gurl.h"

namespace private_ai {

// Recorded in UMA (PrivateAi.Tls.HandshakeStatus); do not reorder.
// LINT.IfChange(TlsHandshakeStatus)
enum class TlsHandshakeStatus {
  kSuccess = 0,
  kCertVerificationFailed = 1,
  kProtocolError = 2,
  kMaxValue = kProtocolError,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivateAiTlsHandshakeStatus)

// Implements SecureChannel using Oak TLS (TLS 1.3 with BoringSSL and AVS
// attestation).
class TlsSecureChannelImpl : public SecureChannel {
 public:
  using TransportFactory =
      base::RepeatingCallback<std::unique_ptr<TlsWebSocketClient>()>;
  using OakSessionFactory =
      base::RepeatingCallback<mojo::Remote<mojom::OakTlsSession>()>;

  class FactoryImpl : public SecureChannel::Factory {
   public:
    FactoryImpl(const GURL& url,
                network::NetworkContextGetter network_context_getter,
                PrivateAiLogger* logger,
                PrivateAiOakSessionDriver* oak_session_driver,
                TlsCertificateRepository* cert_repository,
                std::string expected_server_name,
                std::optional<bool> has_chrome_proxy = std::nullopt);
    ~FactoryImpl() override;

    std::unique_ptr<SecureChannel> Create(base::OnceClosure on_established,
                                          ResponseCallback callback) override;

   private:
    const GURL url_;
    const network::NetworkContextGetter network_context_getter_;
    raw_ptr<PrivateAiLogger> logger_;
    raw_ptr<PrivateAiOakSessionDriver> oak_session_driver_;
    raw_ptr<TlsCertificateRepository> cert_repository_;
    const std::string expected_server_name_;
    const std::optional<bool> has_chrome_proxy_;
  };

  TlsSecureChannelImpl(base::OnceClosure on_established,
                       ResponseCallback callback,
                       TransportFactory transport_factory,
                       OakSessionFactory oak_session_factory,
                       TlsCertificateRepository* cert_repository,
                       PrivateAiLogger* logger,
                       std::string expected_server_name);
  ~TlsSecureChannelImpl() override;

  TlsSecureChannelImpl(const TlsSecureChannelImpl&) = delete;
  TlsSecureChannelImpl& operator=(const TlsSecureChannelImpl&) = delete;

  // SecureChannel:
  bool Write(const Request& request) override;

 private:
  enum class State {
    kFetchingCertificates,
    kStartingHandshake,
    kPerformingHandshake,
    kEstablished,
    kClosed,
  };

  void StartSessionEstablishment(bool force_refresh);
  void OnCertificatesFetched(std::optional<TlsCertificates> certs);
  void OnStartTlsHandshakeDone(
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError> result);
  void OnIncomingFrameReceived(
      base::expected<std::vector<uint8_t>, TlsWebSocketClient::Error> frame);
  void OnProcessHandshakeFrameDone(
      base::expected<mojom::TlsHandshakePtr, mojom::TlsHandshakeError> result);
  void OnOakSessionDisconnected();

  void ProcessPendingRequests();
  void EncryptRequest(const Request& request);
  void OnRequestEncrypted(
      const std::optional<std::vector<uint8_t>>& encrypted_data);
  void OnResponseDecrypted(
      const std::optional<std::vector<uint8_t>>& decrypted_data,
      bool peer_closed);

  void RecordSessionDurationMetrics();
  void FailAllRequestsAndClose(StatusCode status);

  SEQUENCE_CHECKER(sequence_checker_);

  State state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      State::kFetchingCertificates;

  ResponseCallback response_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceClosure on_established_ GUARDED_BY_CONTEXT(sequence_checker_);

  const TransportFactory transport_factory_;
  const OakSessionFactory oak_session_factory_;
  std::unique_ptr<TlsWebSocketClient> transport_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Remote<mojom::OakTlsSession> oak_session_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<TlsCertificateRepository> cert_repository_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<PrivateAiLogger> logger_ GUARDED_BY_CONTEXT(sequence_checker_);

  const std::string expected_server_name_;

  std::vector<Request> pending_requests_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::circular_deque<std::vector<uint8_t>> pending_incoming_frames_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool handshake_frame_in_flight_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool retried_on_cert_error_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::TimeTicks start_time_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<base::TimeTicks> established_time_
      GUARDED_BY_CONTEXT(sequence_checker_);
  int requests_in_session_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::WeakPtrFactory<TlsSecureChannelImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TLS_SECURE_CHANNEL_IMPL_H_

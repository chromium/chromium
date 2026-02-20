// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_SECURE_CHANNEL_IMPL_H_
#define COMPONENTS_PRIVATE_AI_SECURE_CHANNEL_IMPL_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/private_ai/attestation/handler.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/private_ai_common.h"
#include "components/private_ai/secure_channel.h"
#include "components/private_ai/secure_session.h"
#include "components/private_ai/transport.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"
#include "url/gurl.h"

namespace private_ai {

class SecureChannelImpl : public SecureChannel {
 public:
  class FactoryImpl : public SecureChannel::Factory {
   public:
    FactoryImpl(const GURL& url,
                network::mojom::NetworkContext* network_context,
                PrivateAiLogger* logger);
    ~FactoryImpl() override;

    std::unique_ptr<SecureChannel> Create(ResponseCallback callback) override;

   private:
    const GURL url_;
    raw_ptr<network::mojom::NetworkContext> network_context_;
    raw_ptr<PrivateAiLogger> logger_;
  };

  SecureChannelImpl(ResponseCallback callback,
                    std::unique_ptr<Transport> transport,
                    std::unique_ptr<SecureSession> secure_session,
                    std::unique_ptr<AttestationHandler> attestation_handler,
                    PrivateAiLogger* logger);
  ~SecureChannelImpl() override;

  SecureChannelImpl(const SecureChannelImpl&) = delete;
  SecureChannelImpl& operator=(const SecureChannelImpl&) = delete;

  // SecureChannel:
  bool Write(const Request& request) override;

 private:
  // Stages of the secure channel establishment and write process.
  enum class State {
    kPerformingAttestation,
    kWaitingHandshakeMessage,
    kPerformingHandshake,
    kVerifyingHandshake,
    kEstablished,
    kClosed,
  };

  void AddRequestToPendingEncryptionQueue(const Request& request);

  // Helper function to handle state transitions and errors.
  void FailAllRequestsAndClose(ErrorCode error_code);
  void StartSessionEstablishment();
  void ProcessPendingEncryptionRequests();
  void OnRequestEncrypted(
      std::optional<oak::session::v1::EncryptedMessage> encrypted_request);
  void RecordSessionDurationMetrics();

  // Helpers to send and receive data that is converted to proto messages.
  void Send(const oak::session::v1::SessionRequest& request);
  void OnResponseReceived(base::expected<oak::session::v1::SessionResponse,
                                         Transport::TransportError> response);

  // Callbacks for the asynchronous session establishment steps and for sending
  // encrypted requests.
  void OnAttestationResponse(
      const oak::session::v1::SessionResponse& session_response);
  void OnHandshakeMessageReady(
      std::optional<oak::session::v1::HandshakeRequest> handshake_request);
  void OnHandshakeResponse(
      const oak::session::v1::SessionResponse& session_response);
  void OnHandshakeVerification(bool handshake_verified);
  void OnEncryptedResponse(
      const oak::session::v1::SessionResponse& session_response);
  void OnDecryptedResponse(const std::optional<Request>& decrypted_response);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<Transport> transport_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<SecureSession> secure_session_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<AttestationHandler> attestation_handler_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<PrivateAiLogger> logger_ GUARDED_BY_CONTEXT(sequence_checker_);

  State state_ GUARDED_BY_CONTEXT(sequence_checker_) =
      State::kPerformingAttestation;

  ResponseCallback response_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::deque<Request> pending_encryption_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::map<State, base::TimeTicks> state_entry_times_
      GUARDED_BY_CONTEXT(sequence_checker_);
  uint32_t requests_in_session_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::WeakPtrFactory<SecureChannelImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_SECURE_CHANNEL_IMPL_H_

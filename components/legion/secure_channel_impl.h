// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_SECURE_CHANNEL_IMPL_H_
#define COMPONENTS_LEGION_SECURE_CHANNEL_IMPL_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/legion/attestation_handler.h"
#include "components/legion/legion_common.h"
#include "components/legion/secure_channel.h"
#include "components/legion/secure_session.h"
#include "components/legion/transport.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

class SecureChannelImpl : public SecureChannel {
 public:
  SecureChannelImpl(std::unique_ptr<Transport> transport,
                    std::unique_ptr<SecureSession> secure_session,
                    std::unique_ptr<AttestationHandler> attestation_handler);
  ~SecureChannelImpl() override;

  SecureChannelImpl(const SecureChannelImpl&) = delete;
  SecureChannelImpl& operator=(const SecureChannelImpl&) = delete;

  // SecureChannel:
  void SetResponseCallback(ResponseCallback callback) override;
  void EstablishChannel(EstablishChannelCallback callback) override;
  bool Write(const Request& request) override;

 private:
  // Stages of the secure channel establishment and write process.
  enum class State {
    kUninitialized,
    kPerformingAttestation,
    kWaitingHandshakeMessage,
    kPerformingHandshake,
    kVerifyingHandshake,
    kEstablished,
    kClosed,
  };

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
  void OnAttestationResponse(const oak::session::v1::AttestResponse& response);
  void OnHandshakeMessageReady(
      oak::session::v1::HandshakeRequest handshake_request);
  void OnHandshakeResponse(const oak::session::v1::HandshakeResponse& response);
  void OnHandshakeVerification(bool handshake_verified);
  void OnEncryptedResponse(const oak::session::v1::EncryptedMessage& response);
  void OnDecryptedResponse(std::optional<Request> decrypted_response);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<Transport> transport_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<SecureSession> secure_session_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<AttestationHandler> attestation_handler_
      GUARDED_BY_CONTEXT(sequence_checker_);

  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kUninitialized;

  ResponseCallback response_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::deque<Request> pending_encryption_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<EstablishChannelCallback> pending_establishment_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::map<State, base::TimeTicks> state_entry_times_
      GUARDED_BY_CONTEXT(sequence_checker_);
  uint32_t requests_in_session_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::WeakPtrFactory<SecureChannelImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_SECURE_CHANNEL_IMPL_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_SECURE_CHANNEL_IMPL_H_
#define COMPONENTS_LEGION_SECURE_CHANNEL_IMPL_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/legion/attestation_handler.h"
#include "components/legion/legion_common.h"
#include "components/legion/oak_session.h"
#include "components/legion/secure_channel.h"
#include "components/legion/transport.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

class SecureChannelImpl : public SecureChannel {
 public:
  SecureChannelImpl(
      std::unique_ptr<Transport> transport,
      std::unique_ptr<OakSession> oak_session,
      std::unique_ptr<AttestationHandler> attestation_handler);
  ~SecureChannelImpl() override;

  SecureChannelImpl(const SecureChannelImpl&) = delete;
  SecureChannelImpl& operator=(const SecureChannelImpl&) = delete;

  // SecureChannel:
  void Write(Request request, OnResponseReceivedCallback callback) override;

 private:
  struct PendingRequest {
    PendingRequest(Request request, OnResponseReceivedCallback callback);
    ~PendingRequest();

    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);

    Request request;
    OnResponseReceivedCallback callback;
  };

  // Stages of the secure channel establishment and write process.
  enum class State {
    kUninitialized,
    kPerformingAttestation,
    kPerformingHandshake,
    kEstablished,
    kPermanentFailure,
  };

  // Helper function to handle state transitions and errors.
  void FailAllPendingRequests(ResultCode result_code);
  void ResetState();
  void StartSessionEstablishment();
  void ProcessNextRequest();

  // Helpers to send and receive data that is converted to proto messages.
  void Send(const oak::session::v1::SessionRequest& request);
  void OnResponseReceived(base::expected<oak::session::v1::SessionResponse,
                                         Transport::TransportError> response);

  // Callbacks for the asynchronous session establishment steps and for sending
  // encrypted requests.
  void OnAttestationResponse(const oak::session::v1::AttestResponse& response);
  void OnHandshakeResponse(const oak::session::v1::HandshakeResponse& response);
  void OnEncryptedResponse(const oak::session::v1::EncryptedMessage& response);

  std::unique_ptr<Transport> transport_;
  std::unique_ptr<OakSession> oak_session_;
  std::unique_ptr<AttestationHandler> attestation_handler_;

  State state_ = State::kUninitialized;
  bool request_in_flight_ = false;

  std::deque<PendingRequest> pending_requests_;

  base::WeakPtrFactory<SecureChannelImpl> weak_factory_{this};
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_SECURE_CHANNEL_IMPL_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_CONNECTION_TOKEN_ATTESTATION_H_
#define COMPONENTS_LEGION_CONNECTION_TOKEN_ATTESTATION_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/legion/connection.h"
#include "components/legion/phosphor/data_types.h"

namespace private_ai {

namespace phosphor {
class TokenManager;
}

// A decorator for `Connection` that ensures that client attestation request
// is sent first before sending any other requests.
//
// The current implementation does not wait for client attestation response and
// does not handle a situation when the `token_manager` cannot provide token.
class ConnectionTokenAttestation : public Connection {
 public:
  // When `on_disconnect` callback is invoked, all follow-up `Send()` calls will
  // fail immediately without attempting to send a request over the wire.
  ConnectionTokenAttestation(std::unique_ptr<Connection> inner_connection,
                             phosphor::TokenManager* token_manager,
                             base::OnceClosure on_disconnect);
  ~ConnectionTokenAttestation() override;

  ConnectionTokenAttestation(const ConnectionTokenAttestation&) = delete;
  ConnectionTokenAttestation& operator=(const ConnectionTokenAttestation&) =
      delete;

  // Connection override:
  void Send(proto::LegionRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

 private:
  struct PendingRequest {
    PendingRequest(proto::LegionRequest request,
                   base::TimeDelta timeout,
                   OnRequestCallback callback);
    ~PendingRequest();

    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);

    proto::LegionRequest request;
    base::TimeDelta timeout;
    OnRequestCallback callback;
  };

  enum class AttestationState {
    kFetchingToken,
    kWaitingAttestationResponse,
    kSuccess,
    kFailed,
  };

  void FetchToken();
  void OnTokenFetched(std::optional<phosphor::BlindSignedAuthToken> auth_token);
  void OnAttestationResponse(
      base::expected<proto::LegionResponse, ErrorCode> result);

  void FailPendingRequestsAndCallOnDisconnect(ErrorCode error_code);

  std::unique_ptr<Connection> inner_connection_;
  raw_ptr<phosphor::TokenManager> token_manager_;
  base::OnceClosure on_disconnect_;

  AttestationState attestation_state_ = AttestationState::kFetchingToken;
  std::vector<PendingRequest> pending_requests_;

  base::WeakPtrFactory<ConnectionTokenAttestation> weak_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_LEGION_CONNECTION_TOKEN_ATTESTATION_H_

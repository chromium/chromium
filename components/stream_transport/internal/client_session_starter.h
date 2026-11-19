// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_INTERNAL_CLIENT_SESSION_STARTER_H_
#define COMPONENTS_STREAM_TRANSPORT_INTERNAL_CLIENT_SESSION_STARTER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_service.h"
#include "components/stream_transport/public/transport_session.h"
#include "google_apis/gaia/core_account_id.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace signin {
class IdentityManager;
}  // namespace signin

namespace stream_transport {

class TransportChannelImpl;

// An in-flight client session handle returned by `StartClientSession()` before
// sequence 1 completes. Calling `Close()` cancels the in-flight start,
// suppresses the completion callback, and sends an upstream `CloseSession`
// command if sequence 1 already succeeded.
class PendingTransportSessionImpl : public TransportSession {
 public:
  PendingTransportSessionImpl(std::string_view session_id,
                              base::OnceClosure on_close);
  ~PendingTransportSessionImpl() override;

  PendingTransportSessionImpl(const PendingTransportSessionImpl&) = delete;
  PendingTransportSessionImpl& operator=(const PendingTransportSessionImpl&) =
      delete;

  // TransportSession:
  std::string_view GetSessionId() const override;
  base::expected<void, SendUpstreamMessageError> SendUpstreamMessage(
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) override;
  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override;
  void Close() override;

 private:
  const std::string session_id_;
  base::OnceClosure on_close_;
};

// Coordinates the handshake for client-initiated sessions: validates capacity,
// dispatches sequence 1 upstream before the session is registered or watched on
// the downstream stream, and graduates into a matured `TransportSessionImpl`
// upon server acknowledgement.
//
// Owned by `TransportServiceImpl` with lifetime tied to `TransportChannelImpl`.
// Holds a `WeakPtr` to `TransportChannelImpl` so it stops functioning if the
// channel is destroyed.
class ClientSessionStarter {
 public:
  // `channel` and `identity_manager` must be non-null.
  ClientSessionStarter(TransportChannelImpl* channel,
                       signin::IdentityManager* identity_manager);
  ~ClientSessionStarter();

  ClientSessionStarter(const ClientSessionStarter&) = delete;
  ClientSessionStarter& operator=(const ClientSessionStarter&) = delete;

  // Initiates a client-initiated session start handshake. Returns a pending
  // `TransportSession*` handle backed by `PendingTransportSessionImpl`, or
  // `nullptr` if the channel is unavailable or pre-flight validation fails.
  TransportSession* StartClientSession(
      std::string_view session_id,
      PayloadType payload_type,
      const google::protobuf::MessageLite& message,
      TransportService::StartClientSessionCallback callback);

  // Cancels all pending start handshakes and notifies their callbacks with
  // `kChannelUnavailable`. Called when the primary account signs out or
  // changes.
  void AbortPendingStartsForAccountChange();

  // Clears all pending requests and notifies non-cancelled callbacks with
  // `kChannelUnavailable` (used on channel teardown).
  void Clear();

 private:
  // Tracks an in-flight client session start handshake awaiting sequence 1 ack.
  struct PendingStart {
    std::string session_id;
    CoreAccountId account_id;
    std::unique_ptr<PendingTransportSessionImpl> pending_session;
    TransportService::StartClientSessionCallback callback;
    bool cancelled = false;
    bool aborted = false;
  };

  void CancelPendingStart(uint64_t start_id);
  void PostFailedStart(uint64_t start_id, StartClientSessionError error);
  void OnPostFailedStart(uint64_t start_id, StartClientSessionError error);
  void OnInitialMessageSent(uint64_t start_id, int http_status_code);
  void SendUnregisteredCloseSession(std::string_view session_id);

  SEQUENCE_CHECKER(sequence_checker_);

  const base::WeakPtr<TransportChannelImpl> channel_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  uint64_t next_start_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 1;
  base::flat_map<uint64_t, PendingStart> pending_starts_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<ClientSessionStarter> weak_ptr_factory_{this};
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_INTERNAL_CLIENT_SESSION_STARTER_H_

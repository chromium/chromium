// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_INTERNAL_TRANSPORT_CHANNEL_IMPL_H_
#define COMPONENTS_STREAM_TRANSPORT_INTERNAL_TRANSPORT_CHANNEL_IMPL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/stream_transport/internal/transport_message_observer.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/proto_stream/message_stream_client.h"
#include "components/stream_transport/public/transport_channel.h"
#include "components/stream_transport/public/transport_session_registry.h"

namespace browser_actuator {
class ActuatorDownstreamMessage;
class TransportProbe;
}  // namespace browser_actuator

namespace stream_transport {

class ControlTransportHandlerFactory;
class ProbeTransportHandlerFactory;
class StreamConnectionDelegate;
class TransportHandlerFactoryRegistryImpl;
class TransportSessionRegistryImpl;
class UpstreamMessageClient;

// Connection status of the background transport channel downstream stream.
enum class DownstreamConnectionState {
  kDisconnected = 0,
  kConnecting = 1,
  kConnected = 2,
};

// Concrete TransportChannel: the single physical connection shared by every
// session. It is a router, not a session owner — it owns the downstream
// stream client (and observes it) and the session registry, and it moves
// sequence state between them:
//
//  * downstream: as the stream's Observer, it demuxes each message to its
//    session so the *session* can advance its own last-seen position.
//  * (re)connect: it builds the WatchSessionsRequest body from the sessions
//    it can see in the registry.
//  * upstream: it delegates the actual sending of the ActuatorUpstreamMessage
//    to the injected UpstreamMessageClient.
//
// The channel never holds a last-seen number of its own; that lives on each
// TransportSessionImpl. Sessions borrow the channel back (WeakPtr) only to
// hand it their outgoing sends.
class TransportChannelImpl : public TransportChannel,
                             public MessageStreamClient::Observer,
                             public TransportSessionRegistry::Observer {
 public:
  // Builds the fully-decorated downstream stream client (auth wrapper,
  // framer, traffic annotation) around the channel's resume-body delegate.
  // Injected so the channel stays free of network/auth wiring and tests can
  // supply a fake client. The channel adds itself as the client's observer
  // after the factory returns.
  using StreamClientFactory =
      base::OnceCallback<std::unique_ptr<MessageStreamClient>(
          std::unique_ptr<StreamConnectionDelegate> resume_delegate)>;

  explicit TransportChannelImpl(
      std::unique_ptr<UpstreamMessageClient> upstream_message_client,
      StreamClientFactory stream_client_factory = StreamClientFactory());
  ~TransportChannelImpl() override;

  TransportChannelImpl(const TransportChannelImpl&) = delete;
  TransportChannelImpl& operator=(const TransportChannelImpl&) = delete;

  // Registers or unregisters an observer for transport messages. `observer`
  // is not owned and must be removed before it is destroyed.
  void AddObserver(TransportMessageObserver* observer);
  void RemoveObserver(TransportMessageObserver* observer);

  // TransportChannel:
  TransportHandlerFactoryRegistry* GetHandlerFactoryRegistry() override;
  TransportSessionRegistry* GetSessionRegistry() override;
  void SendUpstreamMessage(
      std::string_view session_id,
      PayloadType payload_type,
      const google::protobuf::MessageLite& message,
      std::optional<int64_t> responding_to_sequence_number) override;

  // MessageStreamClient::Observer:
  void OnStreamMessage(const std::string& message) override;
  void OnStreamStatus(const std::string& status) override;
  void OnStreamConnectionStateChange(bool connected) override;

  // TransportSessionRegistry::Observer:
  void OnSessionRegistered(TransportSession* session) override;
  void OnSessionRemoved(std::string_view session_id) override;

  // Tears down the downstream stream, destroys all active sessions, and
  // cancels any in-flight upstream requests when the primary account signs out
  // or switches.
  void ResetForAccountChange();

  DownstreamConnectionState downstream_connection_state() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return downstream_connection_state_;
  }

  base::WeakPtr<TransportChannel> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Test-only: the resume body a (re)connect would send right now.
  std::string BuildWatchSessionsRequestBodyForTesting();

 private:
  // Closes the underlying stream connection.
  void Disconnect();

  // Disconnects (if connected) and reconnects the underlying stream so a
  // fresh WatchSessionsRequest is sent with current session resume positions.
  void ReconnectStream();

  // Sends an upstream `ControlCommand::CloseSession` for `session_id` and
  // destroys the session in `session_registry_`. Copies `session_id` before
  // mutating state so callers may pass `session->GetSessionId()` directly,
  // after which the session pointer must not be dereferenced.
  void CloseAndDestroySession(std::string_view session_id);

  // Forwards a downstream envelope that a session is about to dispatch or
  // drop to every TransportMessageObserver.
  void NotifyDownstreamMessage(
      std::string_view session_id,
      const browser_actuator::ActuatorDownstreamMessage& message,
      DownstreamDeliveryStatus status = DownstreamDeliveryStatus::kSuccess);

  // Forwards the completion HTTP response code for an upstream message to every
  // TransportMessageObserver.
  void NotifyUpstreamResponse(std::string_view session_id,
                              int64_t client_sequence_number,
                              int response_code);

  // Serializes a WatchSessionsRequest describing every session with a known
  // resume position, plus a fresh idempotency id. Handed to the resume
  // delegate as its body provider, so it runs once per connection attempt.
  std::string BuildWatchSessionsRequestBody();

  // Returns the sequence number currently being dispatched on `session_id`, or
  // `std::nullopt` if the session is unknown or idle.
  std::optional<int64_t> GetDispatchingSequenceNumber(
      std::string_view session_id) const;

  // Sends an internal `PayloadType::kProbe` reply for `session_id` carrying
  // `responding_to_sequence_number` (used for immediate and delayed probes).
  base::expected<void, SendUpstreamMessageError> SendProbeReply(
      std::string_view session_id,
      const browser_actuator::TransportProbe& probe,
      std::optional<int64_t> responding_to_sequence_number);

  SEQUENCE_CHECKER(sequence_checker_);

  // The registry holding factories that create the dynamic feature handlers.
  std::unique_ptr<TransportHandlerFactoryRegistryImpl> handler_registry_;

  // Factory for control command handlers (CloseChannel, CloseSession).
  std::unique_ptr<ControlTransportHandlerFactory> control_handler_factory_;

  // Factory for synthetic transport probe handlers.
  std::unique_ptr<ProbeTransportHandlerFactory> probe_handler_factory_;

  // The registry to access and control the life cycles for sessions.
  // Declared before `stream_client_` and after `handler_registry_` to
  // coordinate the correct lifetimes and pointer management.
  std::unique_ptr<TransportSessionRegistryImpl> session_registry_;

  // The underlying network message stream client.
  std::unique_ptr<MessageStreamClient> stream_client_
      GUARDED_BY_CONTEXT(sequence_checker_);

  DownstreamConnectionState downstream_connection_state_ GUARDED_BY_CONTEXT(
      sequence_checker_) = DownstreamConnectionState::kDisconnected;

  // The underlying network client for upstream messages.
  std::unique_ptr<UpstreamMessageClient> upstream_message_client_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::ObserverList<TransportMessageObserver, /*check_empty=*/true> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<TransportChannelImpl> weak_ptr_factory_{this};
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_INTERNAL_TRANSPORT_CHANNEL_IMPL_H_

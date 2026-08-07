// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_CHANNEL_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_CHANNEL_IMPL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/internal/transport/message_stream_client.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_channel.h"
#include "components/browser_actuator/public/transport_session_registry.h"

namespace browser_actuator {

class StreamConnectionDelegate;
class TransportHandlerFactoryRegistryImpl;
class TransportSessionRegistryImpl;

// Concrete TransportChannel: the single physical connection shared by every
// session. It is a router, not a session owner — it owns the downstream
// stream client (and observes it) and the session registry, and it moves
// sequence state between them:
//
//  * downstream: as the stream's Observer, it demuxes each message to its
//    session so the *session* can advance its own last-seen position.
//  * (re)connect: it builds the WatchSessionsRequest body from the sessions
//    it can see in the registry.
//  * upstream: it assembles the ActuatorUpstreamMessage for a session's
//    outgoing send, reading that session's sequence counters.
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
      StreamClientFactory stream_client_factory = StreamClientFactory());
  ~TransportChannelImpl() override;

  TransportChannelImpl(const TransportChannelImpl&) = delete;
  TransportChannelImpl& operator=(const TransportChannelImpl&) = delete;

  // TransportChannel:
  TransportHandlerFactoryRegistry* GetHandlerFactoryRegistry() override;
  TransportSessionRegistry* GetSessionRegistry() override;
  void SendUpstreamMessage(
      std::string_view session_id,
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) override;

  // MessageStreamClient::Observer:
  void OnStreamMessage(const std::string& message) override;
  void OnStreamConnectionStateChange(bool connected) override;

  // TransportSessionRegistry::Observer:
  void OnSessionRegistered(TransportSession* session) override;

  base::WeakPtr<TransportChannel> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Test-only: the resume body a (re)connect would send right now.
  std::string BuildWatchSessionsRequestBodyForTesting();

 private:
  // Serializes a WatchSessionsRequest describing every session with a known
  // resume position, plus a fresh idempotency id. Handed to the resume
  // delegate as its body provider, so it runs once per connection attempt.
  std::string BuildWatchSessionsRequestBody();

  SEQUENCE_CHECKER(sequence_checker_);

  // The registry holding factories that create the dynamic feature handlers.
  std::unique_ptr<TransportHandlerFactoryRegistryImpl> handler_registry_;

  // The registry to access and control the life cycles for sessions.
  // Declared before `stream_client_` and after `handler_registry_` to
  // coordinate the correct lifetimes and pointer management.
  std::unique_ptr<TransportSessionRegistryImpl> session_registry_;

  // The underlying network message stream client.
  std::unique_ptr<MessageStreamClient> stream_client_;

  base::WeakPtrFactory<TransportChannel> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_CHANNEL_IMPL_H_

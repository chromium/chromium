// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TRANSPORT_SESSION_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TRANSPORT_SESSION_IMPL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/transport/downstream_message_sequencer.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/transport_handler_factory_registry.h"
#include "components/browser_actuator/public/transport/transport_session.h"

namespace browser_actuator {
class ActuatorDownstreamTypedPayload;
}  // namespace browser_actuator

namespace browser_actuator::transport {

class TransportChannel;
class TransportHandler;

// Implements TransportSession. Represents an active session instance between
// the server and Chrome instance.
class TransportSessionImpl : public TransportSession,
                             public TransportHandlerFactoryRegistry::Observer {
 public:
  // Number of consecutive downstream messages with invalid sequence numbers
  // before the channel tears down the session.
  static constexpr size_t kMaxConsecutiveInvalidSequenceNumbers = 3;

  // Errors returned from the ProcessPayload method below.
  enum class ProcessPayloadError {
    kChannelDisconnected,
    kNoFactoriesRegistered,
    kHandlerInstantiationFailed,
    kSessionNotFound,
  };

  TransportSessionImpl(std::string_view session_id,
                       base::WeakPtr<TransportChannel> channel);
  ~TransportSessionImpl() override;

  TransportSessionImpl(const TransportSessionImpl&) = delete;
  TransportSessionImpl& operator=(const TransportSessionImpl&) = delete;

  // TransportSession implementation.
  std::string_view GetSessionId() const override;

  base::expected<void, SendUpstreamMessageError> SendUpstreamMessage(
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) override;

  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override;

  void Close() override;

  // TransportHandlerFactoryRegistry::Observer implementation.
  void OnFactoryUnregistered(FactoryId factory_id) override;
  void OnRegistryDestroyed() override;

  base::TimeTicks start_time() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return start_time_;
  }

  int64_t client_sequence_number() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return client_sequence_number_;
  }

  // Routes the serialized bytes of a downstream payload of a given
  // `payload_type` to all active handlers registered to receive it. Handlers
  // are lazily instantiated from the registry on the first message receipt for
  // their payload_type, and each parses `serialized_payload` into its own
  // concrete message type. See TransportHandler for why parsing belongs to the
  // handler rather than to this layer.
  base::expected<void, ProcessPayloadError> ProcessPayload(
      PayloadType payload_type,
      std::string_view serialized_payload);

  // Processes a downstream message from the server by ordering it through the
  // session's sequencer and routing ready payloads to registered handlers.
  base::expected<void, DownstreamMessageSequencer::Error>
  ProcessDownstreamMessage(ActuatorDownstreamMessage message);

  // Last contiguous sequence number (>= 0) dispatched for the session, or -1
  // if no downstream message has been dispatched yet. Used for stream resume
  // positions (when > 0) and upstream reply correlation.
  int64_t last_seen_sequence_number() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return last_seen_sequence_number_;
  }
  bool has_last_seen_sequence_number() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return last_seen_sequence_number_ >= 0;
  }

  size_t consecutive_invalid_sequence_numbers() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return consecutive_invalid_sequence_numbers_;
  }

  // Increments the sequence number for the next outgoing message.
  int64_t IncrementClientSequenceNumber() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ++client_sequence_number_;
  }

  base::WeakPtr<TransportSessionImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Processes a single downstream typed payload.
  // WARNING: may delete `this`, because a handler may destroy the session.
  void ProcessTypedPayload(const ActuatorDownstreamTypedPayload& typed_payload);

  // Resolves and instantiates the handlers registered for a given
  // `payload_type`. If the factories for this payload type have not been
  // instantiated yet for this session, they are resolved from the registry,
  // instantiated, and added to the `routing_table_`.
  base::expected<void, ProcessPayloadError> ResolveHandlersForType(
      PayloadType payload_type);

  // Helper to resolve handlers for `payload_type` and safely dispatch to them
  // using a callback function.
  base::expected<void, ProcessPayloadError> DispatchToHandlers(
      PayloadType payload_type,
      base::FunctionRef<void(TransportHandler*)> dispatch_fn);

  SEQUENCE_CHECKER(sequence_checker_);
  const std::string session_id_;
  base::WeakPtr<TransportChannel> channel_;

  const base::TimeTicks start_time_;
  DownstreamMessageSequencer sequencer_ GUARDED_BY_CONTEXT(sequence_checker_);
  // Tracks the sequence number of the most recently dispatched downstream
  // message (>= 0), or -1 before any message is dispatched. Unlike
  // `sequencer_.last_seen_sequence_number()` (which advances to the end of a
  // drained batch at once), this advances message-by-message during a
  // multi-message drain so synchronous handlers observe the sequence number of
  // the message currently being handled when sending an upstream reply.
  int64_t last_seen_sequence_number_ GUARDED_BY_CONTEXT(sequence_checker_) = -1;
  size_t consecutive_invalid_sequence_numbers_
      GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  int64_t client_sequence_number_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  std::optional<int64_t> dispatching_sequence_number_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Map that holds ownership of all active `TransportHandler` instances for
  // this session, keyed by their instantiating factory ID to guarantee a
  // maximum of one handler per FactoryId is created.
  base::flat_map<FactoryId, std::unique_ptr<TransportHandler>>
      factory_handler_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Routing table mapping each incoming `PayloadType` to the pointers of the
  // active handler instances that support it.
  base::flat_map<PayloadType, std::vector<raw_ptr<TransportHandler>>>
      routing_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::ScopedObservation<TransportHandlerFactoryRegistry,
                          TransportHandlerFactoryRegistry::Observer>
      factory_registry_observation_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  base::WeakPtrFactory<TransportSessionImpl> weak_ptr_factory_{this};
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TRANSPORT_SESSION_IMPL_H_

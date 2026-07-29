// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_SESSION_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_SESSION_IMPL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"

namespace browser_actuator {

class TransportChannel;
class TransportHandler;

// Implements TransportSession. Represents an active session instance between
// the server and Chrome instance.
class TransportSessionImpl : public TransportSession {
 public:
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

  base::expected<void, SendMessageError> SendMessage(
      PayloadType payload_type,
      std::string_view payload) override;

  // Routes a downstream message payload of a given `payload_type` to all active
  // handlers registered to receive it. Handlers are lazily instantiated from
  // the registry on the first message receipt for their payload_type.
  base::expected<void, ProcessPayloadError> ProcessPayload(
      PayloadType payload_type,
      std::string_view payload);

  // Last sequence number for the session received from the server, used for
  // message ordering
  int64_t last_seen_sequence_number() const {
    return last_seen_sequence_number_;
  }
  bool has_last_seen_sequence_number() const {
    return last_seen_sequence_number_ > 0;
  }

  // Returns true if the sequence number is positive and strictly greater than
  // the current sequence number and was recorded.
  [[nodiscard]] bool RecordServerSequenceNumber(int64_t seq);

  int64_t client_sequence_number() const { return client_sequence_number_; }
  // Increments the sequence number for the next outgoing message.
  int64_t IncrementClientSequenceNumber() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ++client_sequence_number_;
  }

  base::WeakPtr<TransportSessionImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Resolves and instantiates the handlers registered for a given
  // `payload_type`. If the factories for this payload type have not been
  // instantiated yet for this session, they are resolved from the registry,
  // instantiated, and added to the `routing_table_`.
  base::expected<void, ProcessPayloadError> ResolveHandlersForType(
      PayloadType payload_type);

  SEQUENCE_CHECKER(sequence_checker_);
  const std::string session_id_;
  base::WeakPtr<TransportChannel> channel_;

  int64_t last_seen_sequence_number_ = 0;
  int64_t client_sequence_number_ = 0;

  // Map that holds ownership of all active `TransportHandler` instances for
  // this session, keyed by their instantiating factory ID to guarantee a
  // maximum of one handler per FactoryId is created.
  base::flat_map<FactoryId, std::unique_ptr<TransportHandler>>
      factory_handler_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Routing table mapping each incoming `PayloadType` to the pointers of the
  // active handler instances that support it.
  base::flat_map<PayloadType, std::vector<raw_ptr<TransportHandler>>>
      routing_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<TransportSessionImpl> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_SESSION_IMPL_H_

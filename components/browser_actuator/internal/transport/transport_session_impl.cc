// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/transport_session_impl.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/transport/metrics_utils.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/public/transport/transport_channel.h"
#include "components/browser_actuator/public/transport/transport_handler.h"
#include "components/browser_actuator/public/transport/transport_handler_factory.h"
#include "components/browser_actuator/public/transport/transport_handler_factory_registry.h"
#include "components/browser_actuator/public/transport/transport_session_registry.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"

namespace browser_actuator::transport {
namespace {

// `Any::Is<T>()` is unavailable for LITE_RUNTIME protos, so `type_url` is
// compared by hand. An empty value is accepted; the server does not always
// populate it.
// TODO(crbug.com/572804026): Require `type_url` once the server always sets
// it.
bool HasExpectedTypeUrl(const ActuatorDownstreamTypedPayload& typed_payload,
                        PayloadType payload_type) {
  const std::string& type_url = typed_payload.proto_payload().type_url();
  return type_url.empty() || type_url == ExpectedTypeUrl(payload_type);
}

// Returns true if `message` carries a `ControlCommand` that tears down the
// session (`kCloseSession`) or channel (`kCloseChannel`). Although payload
// deserialization normally belongs to `TransportHandler` implementations,
// `TransportSessionImpl` intentionally peeks at `ControlCommand` prior to
// sequencing so that a missing earlier sequence number cannot stall teardown.
bool HasTerminatingControlCommand(const ActuatorDownstreamMessage& message) {
  for (const auto& typed_payload : message.typed_payloads()) {
    if (FromDownstreamProtoPayloadType(typed_payload.payload_type()) !=
            PayloadType::kControl ||
        !HasExpectedTypeUrl(typed_payload, PayloadType::kControl)) {
      continue;
    }
    ControlCommand command;
    if (!command.ParseFromString(typed_payload.proto_payload().value())) {
      continue;
    }
    if (command.command_case() == ControlCommand::kCloseSession ||
        command.command_case() == ControlCommand::kCloseChannel) {
      return true;
    }
  }
  return false;
}

}  // namespace

TransportSessionImpl::TransportSessionImpl(
    std::string_view session_id,
    base::WeakPtr<TransportChannel> channel)
    : session_id_(session_id),
      channel_(std::move(channel)),
      start_time_(base::TimeTicks::Now()) {
  DCHECK(channel_);
}

TransportSessionImpl::~TransportSessionImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::string_view TransportSessionImpl::GetSessionId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_id_;
}

base::expected<void, SendUpstreamMessageError>
TransportSessionImpl::SendUpstreamMessage(
    PayloadType payload_type,
    const google::protobuf::MessageLite& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (channel_) {
    std::optional<int64_t> responding_to_sequence_number =
        payload_type == PayloadType::kControl ? dispatching_sequence_number_
                                              : std::nullopt;
    channel_->SendUpstreamMessage(session_id_, payload_type, message,
                                  responding_to_sequence_number);
    return {};
  }
  return base::unexpected(SendUpstreamMessageError::kChannelDisconnected);
}

void TransportSessionImpl::OnMessage(PayloadType payload_type,
                                     std::string_view serialized_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This entry point exists for the out-of-band FCM wake-up path, which
  // carries `sequence_number = 0`. Setting `dispatching_sequence_number_ = 0`
  // ensures synchronous control replies (e.g. `StartSessionAck`) populate
  // `responding_to_sequence_number = 0`.
  base::WeakPtr<TransportSessionImpl> weak_this = GetWeakPtr();
  dispatching_sequence_number_ = 0;
  base::expected<void, ProcessPayloadError> result =
      ProcessPayload(payload_type, serialized_payload);
  if (weak_this) {
    dispatching_sequence_number_.reset();
  }
  if (!result.has_value()) {
    DLOG(WARNING) << "Failed to process payload error: "
                  << static_cast<int>(result.error());
  }
}

void TransportSessionImpl::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel_) {
    return;
  }
  const std::string session_id = session_id_;
  ControlCommand command;
  command.mutable_close_session();
  channel_->SendUpstreamMessage(session_id, PayloadType::kControl, command,
                                /*responding_to_sequence_number=*/std::nullopt);
  if (!channel_) {
    return;
  }
  if (TransportSessionRegistry* registry = channel_->GetSessionRegistry()) {
    registry->DestroySession(session_id);
  }
}

base::expected<void, TransportSessionImpl::ProcessPayloadError>
TransportSessionImpl::ProcessPayload(PayloadType payload_type,
                                     std::string_view serialized_payload) {
  return DispatchToHandlers(payload_type, [payload_type, serialized_payload](
                                              TransportHandler* handler) {
    handler->OnMessage(payload_type, serialized_payload);
  });
}

base::expected<void, TransportSessionImpl::ProcessPayloadError>
TransportSessionImpl::DispatchToHandlers(
    PayloadType payload_type,
    base::FunctionRef<void(TransportHandler*)> dispatch_fn) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel_) {
    return base::unexpected(ProcessPayloadError::kChannelDisconnected);
  }

  auto result = ResolveHandlersForType(payload_type);
  if (!result.has_value()) {
    return result;
  }

  base::WeakPtr<TransportSessionImpl> session = GetWeakPtr();
  auto it = routing_table_.find(payload_type);
  if (it != routing_table_.end()) {
    std::vector<TransportHandler*> handlers;
    handlers.reserve(it->second.size());
    for (const raw_ptr<TransportHandler>& handler : it->second) {
      handlers.push_back(handler.get());
    }
    for (TransportHandler* handler : handlers) {
      if (!session) {
        break;
      }
      auto current_it = routing_table_.find(payload_type);
      if (current_it == routing_table_.end() ||
          !std::ranges::contains(current_it->second, handler)) {
        continue;
      }
      dispatch_fn(handler);
    }
  }

  // WARNING: `this` may have been deleted if `session` is null. Do not access
  // any member variables here.
  return {};
}

base::expected<void, TransportSessionImpl::ProcessPayloadError>
TransportSessionImpl::ResolveHandlersForType(PayloadType payload_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel_) {
    return base::unexpected(ProcessPayloadError::kChannelDisconnected);
  }

  TransportHandlerFactoryRegistry* registry =
      channel_->GetHandlerFactoryRegistry();
  CHECK(registry);
  if (!factory_registry_observation_.IsObserving()) {
    factory_registry_observation_.Observe(registry);
  }

  std::vector<TransportHandlerFactory*> factories =
      registry->GetFactories(payload_type);
  if (factories.empty()) {
    return base::unexpected(ProcessPayloadError::kNoFactoriesRegistered);
  }

  bool has_active_handler = false;
  base::WeakPtr<TransportSessionImpl> session = GetWeakPtr();
  for (TransportHandlerFactory* factory : factories) {
    FactoryId factory_id = factory->GetFactoryId();
    if (factory_handler_map_.contains(factory_id)) {
      has_active_handler = true;
      continue;  // Previously instantiated
    }

    std::unique_ptr<TransportHandler> handler = factory->OnNewSession(this);
    if (!session) {
      return base::unexpected(ProcessPayloadError::kSessionNotFound);
    }
    if (!handler) {
      continue;
    }
    has_active_handler = true;
    for (PayloadType supported_type : factory->GetSupportedPayloadTypes()) {
      routing_table_[supported_type].push_back(handler.get());
    }
    factory_handler_map_[factory_id] = std::move(handler);
  }

  if (!has_active_handler) {
    return base::unexpected(ProcessPayloadError::kHandlerInstantiationFailed);
  }

  return {};
}

void TransportSessionImpl::OnFactoryUnregistered(FactoryId factory_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto handler_it = factory_handler_map_.find(factory_id);
  if (handler_it == factory_handler_map_.end()) {
    return;
  }

  std::unique_ptr<TransportHandler> handler = std::move(handler_it->second);
  factory_handler_map_.erase(handler_it);

  for (auto routing_it = routing_table_.begin();
       routing_it != routing_table_.end();) {
    std::erase(routing_it->second, handler.get());
    if (routing_it->second.empty()) {
      routing_it = routing_table_.erase(routing_it);
    } else {
      ++routing_it;
    }
  }
}

void TransportSessionImpl::OnRegistryDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  factory_registry_observation_.Reset();
}

base::expected<void, DownstreamMessageSequencer::Error>
TransportSessionImpl::ProcessDownstreamMessage(
    ActuatorDownstreamMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool is_new_terminating_command =
      message.sequence_number() > sequencer_.last_seen_sequence_number() &&
      HasTerminatingControlCommand(message);

  std::vector<ActuatorDownstreamMessage> deliverable;
  if (is_new_terminating_command) {
    consecutive_invalid_sequence_numbers_ = 0;
    // Flush any buffered messages and bypass capacity-bounded buffering so a
    // full reorder buffer cannot reject session or channel teardown.
    deliverable = sequencer_.Flush();
    auto it = std::upper_bound(
        deliverable.begin(), deliverable.end(), message.sequence_number(),
        [](int64_t seq, const ActuatorDownstreamMessage& msg) {
          return seq < msg.sequence_number();
        });
    deliverable.insert(it, std::move(message));
  } else {
    base::expected<std::vector<ActuatorDownstreamMessage>,
                   DownstreamMessageSequencer::Error>
        ready_messages = sequencer_.AddMessage(std::move(message));
    if (!ready_messages.has_value()) {
      if (ready_messages.error() ==
          DownstreamMessageSequencer::Error::kInvalidSequenceNumber) {
        ++consecutive_invalid_sequence_numbers_;
      }
      return base::unexpected(ready_messages.error());
    }
    consecutive_invalid_sequence_numbers_ = 0;
    deliverable = std::move(*ready_messages);
  }

  base::WeakPtr<TransportSessionImpl> weak_this = GetWeakPtr();
  for (const auto& ready_message : deliverable) {
    if (!weak_this) {
      break;
    }
    last_seen_sequence_number_ = ready_message.sequence_number();
    dispatching_sequence_number_ = ready_message.sequence_number();
    for (const auto& typed_payload : ready_message.typed_payloads()) {
      // A handler's OnMessage() may have synchronously destroyed this session.
      if (!weak_this) {
        break;
      }
      ProcessTypedPayload(typed_payload);
    }
  }
  if (weak_this) {
    dispatching_sequence_number_.reset();
  }
  return {};
}

void TransportSessionImpl::ProcessTypedPayload(
    const ActuatorDownstreamTypedPayload& typed_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<PayloadType> payload_type =
      FromDownstreamProtoPayloadType(typed_payload.payload_type());
  if (!payload_type.has_value()) {
    DLOG(WARNING) << "Ignoring payload with unspecified or unknown type: "
                  << typed_payload.payload_type();
    RecordDownstreamPayloadDropped(DownstreamPayloadDropReason::kUnknownType);
    return;
  }

  if (!HasExpectedTypeUrl(typed_payload, *payload_type)) {
    DLOG(WARNING) << "Ignoring payload with unexpected type_url: "
                  << typed_payload.proto_payload().type_url()
                  << ", expected: " << ExpectedTypeUrl(*payload_type);
    RecordDownstreamPayloadDropped(
        DownstreamPayloadDropReason::kTypeUrlMismatch);
    return;
  }

  auto result =
      ProcessPayload(*payload_type, typed_payload.proto_payload().value());
  // WARNING: `this` may have been deleted by a handler. Do not access any
  // member variables here.
  if (!result.has_value()) {
    DLOG(WARNING) << "Failed to process payload "
                  << "error: " << static_cast<int>(result.error());
    RecordDownstreamPayloadDropped(
        result.error() == ProcessPayloadError::kNoFactoriesRegistered
            ? DownstreamPayloadDropReason::kNoHandler
            : DownstreamPayloadDropReason::kDispatchFailed);
  }
}

}  // namespace browser_actuator::transport

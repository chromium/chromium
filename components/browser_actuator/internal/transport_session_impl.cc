// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_session_impl.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/transport_channel.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/browser_actuator/public/transport_handler_factory_registry.h"

namespace browser_actuator {

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
    channel_->SendUpstreamMessage(session_id_, payload_type, message);
    return {};
  }
  return base::unexpected(SendUpstreamMessageError::kChannelDisconnected);
}

void TransportSessionImpl::OnMessage(
    PayloadType payload_type,
    const google::protobuf::MessageLite& message) {
  // This entry point exists for the FCM wake-up path, which receives an
  // already-parsed proto because the sharing stack parses the enclosing
  // SharingMessage before this layer ever sees it. Handlers consume bytes, so
  // serialize here rather than give every handler a second, parsed overload to
  // implement. The round-trip is confined to wake-ups, which arrive at most
  // once per session.
  // TODO(crbug.com/538161953): Once the FCM flow delivers an
  // ActuatorDownstreamMessage through ProcessDownstreamMessage, this method
  // and its round-trip can be deleted along with TransportSession::OnMessage.
  const std::string serialized_payload = message.SerializeAsString();
  std::ignore = ProcessPayload(payload_type, serialized_payload);
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

bool TransportSessionImpl::RecordServerSequenceNumber(int64_t seq) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Sequence numbers begin at 1 and can only be strictly increasing.
  if (seq > 0 && seq > last_seen_sequence_number_) {
    last_seen_sequence_number_ = seq;
    return true;
  }
  return false;
}

void TransportSessionImpl::ProcessDownstreamMessage(
    const ActuatorDownstreamMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!RecordServerSequenceNumber(message.sequence_number())) {
    return;
  }

  base::WeakPtr<TransportSessionImpl> weak_this = GetWeakPtr();
  for (const auto& typed_payload : message.typed_payloads()) {
    if (!weak_this) {
      break;
    }
    // Map ActuatorDownstreamPayloadType to public PayloadType
    switch (typed_payload.payload_type()) {
      case ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND: {
        auto result = ProcessPayload(PayloadType::kControl,
                                     typed_payload.proto_payload().value());
        if (!result.has_value()) {
          DLOG(WARNING) << "Failed to process payload "
                        << "error: " << static_cast<int>(result.error());
        }
        break;
      }

      case ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED:
      default:
        // Ignore unspecified or unknown payload types
        DLOG(WARNING) << "Ignoring payload with unspecified or unknown type: "
                      << typed_payload.payload_type();
        continue;
    }
  }
}

}  // namespace browser_actuator

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/transport_channel_impl.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/browser_actuator/internal/transport/control_transport_handler.h"
#include "components/browser_actuator/internal/transport/downstream_message_sequencer.h"
#include "components/browser_actuator/internal/transport/probe_transport_handler.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/resume_body_connection_delegate.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport/transport_handler_factory_registry_impl.h"
#include "components/browser_actuator/internal/transport/transport_message_observer.h"
#include "components/browser_actuator/internal/transport/transport_session_impl.h"
#include "components/browser_actuator/internal/transport/transport_session_registry_impl.h"
#include "components/browser_actuator/internal/transport/upstream_message_client/upstream_message_client.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"

namespace browser_actuator::transport {

TransportChannelImpl::TransportChannelImpl(
    std::unique_ptr<UpstreamMessageClient> upstream_message_client,
    StreamClientFactory stream_client_factory)
    : upstream_message_client_(std::move(upstream_message_client)) {
  CHECK(upstream_message_client_);
  handler_registry_ = std::make_unique<TransportHandlerFactoryRegistryImpl>();
  session_registry_ = std::make_unique<TransportSessionRegistryImpl>(
      weak_ptr_factory_.GetWeakPtr(),
      base::BindRepeating(
          [](base::WeakPtr<TransportChannelImpl> self,
             std::string_view session_id,
             const ActuatorDownstreamMessage& message) {
            if (self) {
              self->NotifyDownstreamMessage(session_id, message,
                                            DownstreamDeliveryStatus::kSuccess);
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
  session_registry_->AddObserver(this);

  control_handler_factory_ = std::make_unique<ControlTransportHandlerFactory>(
      base::BindRepeating(&TransportChannelImpl::Disconnect,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&TransportSessionRegistryImpl::DestroySession,
                          session_registry_->GetWeakPtr()));
  handler_registry_->RegisterFactory(control_handler_factory_.get());

  probe_handler_factory_ = std::make_unique<ProbeTransportHandlerFactory>(
      base::BindRepeating(
          [](base::WeakPtr<TransportChannelImpl> self,
             std::string_view session_id) -> std::optional<int64_t> {
            return self ? self->GetDispatchingSequenceNumber(session_id)
                        : std::nullopt;
          },
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          [](base::WeakPtr<TransportChannelImpl> self,
             std::string_view session_id, const TransportProbe& probe,
             std::optional<int64_t> responding_to_sequence_number)
              -> base::expected<void, SendUpstreamMessageError> {
            if (!self) {
              return base::unexpected(
                  SendUpstreamMessageError::kChannelDisconnected);
            }
            return self->SendProbeReply(session_id, probe,
                                        responding_to_sequence_number);
          },
          weak_ptr_factory_.GetWeakPtr()));
  handler_registry_->RegisterFactory(probe_handler_factory_.get());

  // The resume delegate carries no state: on every connection attempt it asks
  // the channel to rebuild the WatchSessionsRequest body from the current
  // sessions. Unretained is safe because the channel owns the client — and
  // thus the delegate holding this callback — and tears it down first, so the
  // callback never runs after `this` is gone. (A WeakPtr can't be used here:
  // it may not bind to a method that returns a value.)
  if (stream_client_factory) {
    auto resume_delegate = std::make_unique<ResumeBodyConnectionDelegate>(
        base::BindRepeating(
            &TransportChannelImpl::BuildWatchSessionsRequestBody,
            base::Unretained(this)),
        std::make_unique<DefaultStreamConnectionDelegate>());

    stream_client_ =
        std::move(stream_client_factory).Run(std::move(resume_delegate));
    if (stream_client_) {
      stream_client_->AddObserver(this);
    }
  }
}

TransportChannelImpl::~TransportChannelImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_registry_) {
    session_registry_->RemoveObserver(this);
  }
  if (handler_registry_ && probe_handler_factory_) {
    handler_registry_->UnregisterFactory(probe_handler_factory_.get());
  }
  if (handler_registry_ && control_handler_factory_) {
    handler_registry_->UnregisterFactory(control_handler_factory_.get());
  }
  if (stream_client_) {
    stream_client_->RemoveObserver(this);
  }
}

void TransportChannelImpl::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  downstream_connection_state_ = DownstreamConnectionState::kDisconnected;
  if (stream_client_) {
    stream_client_->Disconnect();
  }
}

void TransportChannelImpl::ResetForAccountChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
  if (session_registry_) {
    session_registry_->Clear();
  }
  if (upstream_message_client_) {
    upstream_message_client_->CancelPendingRequests();
  }
}

TransportHandlerFactoryRegistry*
TransportChannelImpl::GetHandlerFactoryRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handler_registry_.get();
}

TransportSessionRegistry* TransportChannelImpl::GetSessionRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_registry_.get();
}

void TransportChannelImpl::AddObserver(TransportMessageObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void TransportChannelImpl::RemoveObserver(TransportMessageObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void TransportChannelImpl::OnSessionRegistered(TransportSession*) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ReconnectStream();
}

void TransportChannelImpl::OnSessionRemoved(std::string_view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (session_registry_->GetAllSessionImpls().empty()) {
    Disconnect();
  } else {
    ReconnectStream();
  }
}

void TransportChannelImpl::ReconnectStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_client_) {
    if (stream_client_->IsConnected()) {
      stream_client_->Disconnect();
    }
    downstream_connection_state_ = DownstreamConnectionState::kConnecting;
    stream_client_->Connect();
  }
}

void TransportChannelImpl::CloseAndDestroySession(std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string owned_session_id(session_id);
  ControlCommand command;
  command.mutable_close_session();
  SendUpstreamMessage(owned_session_id, PayloadType::kControl, command,
                      /*responding_to_sequence_number=*/std::nullopt);
  session_registry_->DestroySession(owned_session_id);
}

// Downstream: route each message to its session so the *session* advances its
// own resume position.
void TransportChannelImpl::OnStreamMessage(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WatchSessionsResponse response;
  if (!response.ParseFromString(message) ||
      !response.has_actuator_downstream_message()) {
    DLOG(WARNING) << "Failed to parse WatchSessionsResponse from stream";
    return;
  }
  ActuatorDownstreamMessage* downstream =
      response.mutable_actuator_downstream_message();
  if (downstream->session_id().empty()) {
    DLOG(WARNING) << "Received ActuatorDownstreamMessage with empty session_id";
    return;
  }

  TransportSessionImpl* session =
      session_registry_->GetSessionImpl(downstream->session_id());
  if (!session) {
    DVLOG(1) << "Ignoring downstream message for unknown or closed session: "
             << downstream->session_id();
    return;
  }

  const int64_t sequence_number = downstream->sequence_number();
  std::optional<ActuatorDownstreamMessage> dropped_copy;
  if (!observers_.empty()) {
    dropped_copy = *downstream;
  }
  base::expected<void, DownstreamMessageSequencer::Error> result =
      session->ProcessDownstreamMessage(std::move(*downstream));
  if (!result.has_value()) {
    switch (result.error()) {
      case DownstreamMessageSequencer::Error::kBufferFull:
        if (session->consecutive_buffer_full_errors() >=
            TransportSessionImpl::kMaxConsecutiveBufferFullErrors) {
          DLOG(WARNING) << "Tearing down session " << session->GetSessionId()
                        << " after receiving "
                        << session->consecutive_buffer_full_errors()
                        << " consecutive reorder-buffer overflows without "
                           "advancing sequence cursor";
          CloseAndDestroySession(session->GetSessionId());
          break;
        }
        DVLOG(1) << "Downstream reorder buffer full for session "
                 << session->GetSessionId()
                 << "; reconnecting stream to request replay from "
                 << session->last_seen_sequence_number();
        if (dropped_copy.has_value()) {
          NotifyDownstreamMessage(session->GetSessionId(), *dropped_copy,
                                  DownstreamDeliveryStatus::kFailure);
        }
        ReconnectStream();
        break;
      case DownstreamMessageSequencer::Error::kInvalidSequenceNumber:
        DLOG(WARNING) << "Ignoring downstream message with invalid sequence "
                         "number "
                      << sequence_number << " for session "
                      << session->GetSessionId() << " (consecutive count: "
                      << session->consecutive_invalid_sequence_numbers() << ")";
        if (session->consecutive_invalid_sequence_numbers() >=
            TransportSessionImpl::kMaxConsecutiveInvalidSequenceNumbers) {
          DLOG(WARNING) << "Tearing down session " << session->GetSessionId()
                        << " after receiving "
                        << session->consecutive_invalid_sequence_numbers()
                        << " consecutive invalid sequence numbers";
          CloseAndDestroySession(session->GetSessionId());
        }
        break;
    }
  }
}

void TransportChannelImpl::OnStreamStatus(const std::string& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  downstream_connection_state_ = DownstreamConnectionState::kDisconnected;
}

void TransportChannelImpl::OnStreamConnectionStateChange(bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/534398806): surface connection state to sessions/handlers if
  // needed.
  downstream_connection_state_ = connected
                                     ? DownstreamConnectionState::kConnected
                                     : DownstreamConnectionState::kDisconnected;
}

// Upstream: assemble the outgoing message for a session, advancing that
// session's client sequence counter.
void TransportChannelImpl::SendUpstreamMessage(
    std::string_view session_id,
    PayloadType payload_type,
    const google::protobuf::MessageLite& message,
    std::optional<int64_t> responding_to_sequence_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransportSessionImpl* session = session_registry_->GetSessionImpl(session_id);
  if (!session) {
    return;
  }

  int64_t client_sequence_number = session->IncrementClientSequenceNumber();
  std::optional<int64_t> validated_responding_to_seq;
  if (responding_to_sequence_number.has_value() &&
      *responding_to_sequence_number >= 0) {
    validated_responding_to_seq = *responding_to_sequence_number;
  }
  // SendUpstreamMessage can invoke its callback synchronously on immediate
  // OAuth failure before returning `upstream`, so SyncSendState buffers the
  // status code to guarantee `http_response_code = 0` is notified first.
  struct SyncSendState {
    bool in_sync_call = true;
    std::optional<int> response_code;
  };
  auto sync_state = base::MakeRefCounted<base::RefCountedData<SyncSendState>>();

  ActuatorUpstreamMessage upstream =
      upstream_message_client_->SendUpstreamMessage(
          session_id, client_sequence_number, validated_responding_to_seq,
          payload_type, message,
          base::BindOnce(
              [](base::WeakPtr<TransportChannelImpl> self,
                 scoped_refptr<base::RefCountedData<SyncSendState>> sync_state,
                 std::string session_id, int64_t client_sequence_number,
                 bool success, int response_code) {
                if (!success) {
                  VLOG(1) << "Failed to send upstream message for session "
                          << session_id << " (response code: " << response_code
                          << ")";
                  // TODO(crbug.com/532661039): Consider adding retry buffering
                  // or closing the channel.
                }
                if (sync_state->data.in_sync_call) {
                  sync_state->data.response_code = response_code;
                  return;
                }
                if (self) {
                  self->NotifyUpstreamResponse(
                      session_id, client_sequence_number, response_code);
                }
              },
              weak_ptr_factory_.GetWeakPtr(), sync_state,
              std::string(session_id), client_sequence_number));

  sync_state->data.in_sync_call = false;
  observers_.Notify(&TransportMessageObserver::OnUpstreamMessage,
                    std::string_view(upstream.session_id()), upstream,
                    /*http_response_code=*/0);
  if (sync_state->data.response_code.has_value()) {
    NotifyUpstreamResponse(upstream.session_id(), client_sequence_number,
                           *sync_state->data.response_code);
  }
}

void TransportChannelImpl::NotifyDownstreamMessage(
    std::string_view session_id,
    const ActuatorDownstreamMessage& message,
    DownstreamDeliveryStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.Notify(&TransportMessageObserver::OnDownstreamMessage, session_id,
                    message, status);
}

void TransportChannelImpl::NotifyUpstreamResponse(
    std::string_view session_id,
    int64_t client_sequence_number,
    int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observers_.empty()) {
    return;
  }
  ActuatorUpstreamMessage response_msg;
  response_msg.set_session_id(std::string(session_id));
  response_msg.set_client_sequence_number(client_sequence_number);
  observers_.Notify(&TransportMessageObserver::OnUpstreamMessage, session_id,
                    response_msg, response_code);
}

std::string TransportChannelImpl::BuildWatchSessionsRequestBody() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WatchSessionsRequest request;
  // Fresh per attempt: each (re)connect is a distinct logical watch.
  request.set_request_id(base::Uuid::GenerateRandomV4().AsLowercaseString());

  for (TransportSessionImpl* session :
       session_registry_->GetAllSessionImpls()) {
    WatchSessionsRequest::Session* data = request.add_sessions();
    data->set_session_id(session->GetSessionId());
    if (session->last_seen_sequence_number() > 0) {
      data->set_last_seen_sequence_number(session->last_seen_sequence_number());
    }
  }
  return request.SerializeAsString();
}

std::string TransportChannelImpl::BuildWatchSessionsRequestBodyForTesting() {
  return BuildWatchSessionsRequestBody();
}

std::optional<int64_t> TransportChannelImpl::GetDispatchingSequenceNumber(
    std::string_view session_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransportSessionImpl* session = session_registry_->GetSessionImpl(session_id);
  return session ? session->dispatching_sequence_number() : std::nullopt;
}

base::expected<void, SendUpstreamMessageError>
TransportChannelImpl::SendProbeReply(
    std::string_view session_id,
    const TransportProbe& probe,
    std::optional<int64_t> responding_to_sequence_number) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session_registry_->GetSessionImpl(session_id)) {
    return base::unexpected(SendUpstreamMessageError::kChannelDisconnected);
  }
  SendUpstreamMessage(session_id, PayloadType::kProbe, probe,
                      responding_to_sequence_number);
  return {};
}

}  // namespace browser_actuator::transport

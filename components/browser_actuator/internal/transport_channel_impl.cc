// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_channel_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/resume_body_connection_delegate.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"
#include "components/browser_actuator/internal/transport_session_impl.h"
#include "components/browser_actuator/internal/transport_session_registry_impl.h"

namespace browser_actuator {

TransportChannelImpl::TransportChannelImpl(
    StreamClientFactory stream_client_factory) {
  session_registry_ = std::make_unique<TransportSessionRegistryImpl>(
      weak_ptr_factory_.GetWeakPtr());

  // The resume delegate carries no state: on every connection attempt it asks
  // the channel to rebuild the WatchSessionsRequest body from the current
  // sessions. Unretained is safe because the channel owns the client — and
  // thus the delegate holding this callback — and tears it down first, so the
  // callback never runs after `this` is gone. (A WeakPtr can't be used here:
  // it may not bind to a method that returns a value.)
  auto resume_delegate = std::make_unique<ResumeBodyConnectionDelegate>(
      base::BindRepeating(&TransportChannelImpl::BuildWatchSessionsRequestBody,
                          base::Unretained(this)),
      std::make_unique<DefaultStreamConnectionDelegate>());

  stream_client_ =
      std::move(stream_client_factory).Run(std::move(resume_delegate));
  stream_client_->AddObserver(this);
}

TransportChannelImpl::~TransportChannelImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_client_) {
    stream_client_->RemoveObserver(this);
  }
}

TransportHandlerFactoryRegistry*
TransportChannelImpl::GetHandlerFactoryRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/532660606): own and return the handler factory registry.
  return nullptr;
}

TransportSessionRegistry* TransportChannelImpl::GetSessionRegistry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_registry_.get();
}

// Downstream: route each message to its session so the *session* advances its
// own resume position.
void TransportChannelImpl::OnStreamMessage(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ActuatorDownstreamMessage downstream;
  if (!downstream.ParseFromString(message) || downstream.session_id().empty()) {
    DLOG(WARNING) << "Failed to parse downstream message or missing session_id";
    return;
  }

  TransportSessionImpl* session =
      session_registry_->GetOrCreateSession(downstream.session_id());
  if (!session) {
    return;
  }
  if (!session->RecordServerSequenceNumber(downstream.sequence_number())) {
    return;
  }

  // TODO(crbug.com/532660606): route downstream.typed_payloads() to the
  // handler for each payload_type.
}

void TransportChannelImpl::OnStreamConnectionStateChange(bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/534398806): surface connection state to sessions/handlers if
  // needed.
}

// Upstream: assemble the outgoing message for a session, reading that
// session's own counters.
void TransportChannelImpl::SendUpstreamMessage(std::string_view session_id,
                                               PayloadType payload_type,
                                               std::string_view payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransportSessionImpl* session = session_registry_->GetSessionImpl(session_id);
  if (!session) {
    return;
  }

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id(session_id);
  upstream.set_client_sequence_number(session->IncrementClientSequenceNumber());
  // Only correlate to a real received message.
  if (session->has_last_seen_sequence_number()) {
    upstream.set_responding_to_sequence_number(
        session->last_seen_sequence_number());
  }

  // TODO(crbug.com/532660606): pack `payload` + `payload_type` into a
  // typed_payload (google.protobuf.Any) once the outgoing payload proto lands.
  // TODO(crbug.com/532660606): POST upstream.SerializeAsString() to the
  // upstream endpoint (SimpleURLLoader::AttachStringForUpload,
  // "application/x-protobuf").
}

std::string TransportChannelImpl::BuildWatchSessionsRequestBody() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  WatchSessionsRequest request;
  // Fresh per attempt: each (re)connect is a distinct logical watch.
  request.set_request_id(base::Uuid::GenerateRandomV4().AsLowercaseString());

  for (TransportSessionImpl* session :
       session_registry_->GetAllSessionImpls()) {
    if (!session->has_last_seen_sequence_number()) {
      continue;  // Nothing received yet; no resume position to send.
    }
    WatchSessionsRequest::Session* data = request.add_sessions();
    data->set_session_id(session->GetSessionId());
    data->set_last_seen_sequence_number(session->last_seen_sequence_number());
  }
  return request.SerializeAsString();
}

std::string TransportChannelImpl::BuildWatchSessionsRequestBodyForTesting() {
  return BuildWatchSessionsRequestBody();
}

base::WeakPtr<TransportChannelImpl> TransportChannelImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace browser_actuator

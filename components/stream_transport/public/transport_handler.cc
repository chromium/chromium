// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/public/transport_handler.h"

#include "components/stream_transport/public/transport_session.h"

namespace stream_transport {

TransportHandler::TransportHandler(TransportSession* session)
    : session_(session) {}

TransportHandler::~TransportHandler() = default;

base::expected<void, SendUpstreamMessageError>
TransportHandler::SendUpstreamMessage(
    PayloadType payload_type,
    const google::protobuf::MessageLite& message) {
  if (!session_) {
    return base::unexpected(SendUpstreamMessageError::kChannelDisconnected);
  }
  return session_->SendUpstreamMessage(payload_type, message);
}

void TransportHandler::CloseSession() {
  if (session_) {
    session_->Close();
  }
}

}  // namespace stream_transport

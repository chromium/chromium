// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/public/transport_handler.h"

#include "components/browser_actuator/public/transport_session.h"

namespace browser_actuator {

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

}  // namespace browser_actuator

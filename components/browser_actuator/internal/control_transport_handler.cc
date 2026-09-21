// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/control_transport_handler.h"

#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/transport_session.h"

namespace browser_actuator {

ControlTransportHandler::ControlTransportHandler(
    TransportSession* session,
    CloseChannelCallback close_channel_cb,
    CloseSessionCallback close_session_cb)
    : TransportHandler(session),
      close_channel_cb_(std::move(close_channel_cb)),
      close_session_cb_(std::move(close_session_cb)) {}

ControlTransportHandler::~ControlTransportHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ControlTransportHandler::OnMessage(PayloadType payload_type,
                                        std::string_view serialized_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ControlCommand command;
  if (!command.ParseFromString(serialized_payload)) {
    DLOG(WARNING) << "Failed to parse ControlCommand payload";
    return;
  }

  switch (command.command_case()) {
    case ControlCommand::kCloseChannel: {
      if (close_channel_cb_) {
        close_channel_cb_.Run();
      }
      break;
    }
    case ControlCommand::kCloseSession: {
      if (close_session_cb_ && session()) {
        std::string session_id(session()->GetSessionId());
        close_session_cb_.Run(session_id);
      }
      break;
    }
    case ControlCommand::kStartSession:
      // StartSession is primarily used for FCM wakeup and establishing the
      // connection. If received over the stream, it might be redundant or
      // used for logging/tracing.
      // TODO: Implement handling if needed over stream.
      break;
    case ControlCommand::COMMAND_NOT_SET:
      break;
  }
}

ControlTransportHandlerFactory::ControlTransportHandlerFactory(
    ControlTransportHandler::CloseChannelCallback close_channel_cb,
    ControlTransportHandler::CloseSessionCallback close_session_cb)
    : close_channel_cb_(std::move(close_channel_cb)),
      close_session_cb_(std::move(close_session_cb)) {}

ControlTransportHandlerFactory::~ControlTransportHandlerFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FactoryId ControlTransportHandlerFactory::GetFactoryId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return FactoryId::kControl;
}

std::vector<PayloadType>
ControlTransportHandlerFactory::GetSupportedPayloadTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {PayloadType::kControl};
}

std::unique_ptr<TransportHandler> ControlTransportHandlerFactory::OnNewSession(
    TransportSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<ControlTransportHandler>(session, close_channel_cb_,
                                                   close_session_cb_);
}

}  // namespace browser_actuator

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/control_transport_handler.h"

#include <utility>

#include "base/logging.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/transport_session.h"

namespace browser_actuator {

ControlTransportHandler::ControlTransportHandler(
    std::string_view session_id,
    CloseChannelCallback close_channel_cb,
    CloseSessionCallback close_session_cb)
    : session_id_(session_id),
      close_channel_cb_(std::move(close_channel_cb)),
      close_session_cb_(std::move(close_session_cb)) {}

ControlTransportHandler::~ControlTransportHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ControlTransportHandler::OnMessage(std::string_view payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ControlCommand command;
  if (!command.ParseFromArray(payload.data(), payload.size())) {
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
      if (close_session_cb_) {
        close_session_cb_.Run(session_id_);
      }
      break;
    }
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
  std::string_view session_id = session ? session->GetSessionId() : "";
  return std::make_unique<ControlTransportHandler>(
      session_id, close_channel_cb_, close_session_cb_);
}

}  // namespace browser_actuator

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/named_message_port_connector/named_message_port_connector.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cast/message_port/platform_message_port.h"

namespace cast_api_bindings {

NamedMessagePortConnector::NamedMessagePortConnector() = default;

NamedMessagePortConnector::~NamedMessagePortConnector() = default;

void NamedMessagePortConnector::RegisterPortHandler(
    PortConnectedCallback handler) {
  handler_ = std::move(handler);
}

// Receives the MessagePort and forwards ports to their corresponding binding
// handlers.
bool NamedMessagePortConnector::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<MessagePort>> ports) {
  if (ports.size() != 1) {
    DLOG(FATAL) << "Only one control port should be provided";
    return false;
  }

  // Read the port ID.
  if (message.empty()) {
    DLOG(FATAL) << "No port ID was specified.";
    return false;
  }

  return handler_.Run(message, std::move(ports[0]));
}

void NamedMessagePortConnector::OnPipeError() {}

void NamedMessagePortConnector::GetConnectMessage(
    std::string* message,
    std::unique_ptr<MessagePort>* port) {
  constexpr char kControlPortConnectMessage[] = "cast.master.connect";
  CreatePlatformMessagePortPair(&control_port_, port);
  *message = kControlPortConnectMessage;
  control_port_->SetReceiver(this);
}

}  // namespace cast_api_bindings

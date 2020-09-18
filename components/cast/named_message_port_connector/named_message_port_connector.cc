// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast/named_message_port_connector/named_message_port_connector.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

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
    blink::WebMessagePort::Message message) {
  if (message.ports.size() != 1) {
    DLOG(FATAL) << "Only one control port should be provided";
    return false;
  }

  // Read the port ID.
  base::string16 data_utf16 = std::move(message.data);
  std::string binding_id;
  if (!base::UTF16ToUTF8(data_utf16.data(), data_utf16.size(), &binding_id))
    return false;

  return handler_.Run(binding_id, std::move(message.ports[0]));
}

blink::WebMessagePort::Message NamedMessagePortConnector::GetConnectMessage() {
  constexpr char kControlPortConnectMessage[] = "cast.master.connect";

  // Pass the control message port into the page as an HTML5 MessageChannel
  // message.
  auto port_pair = blink::WebMessagePort::CreatePair();

  control_port_ = std::move(port_pair.first);
  control_port_.SetReceiver(this, base::ThreadTaskRunnerHandle::Get());

  blink::WebMessagePort::Message connect_message;
  connect_message.data = base::UTF8ToUTF16(kControlPortConnectMessage);
  connect_message.ports.push_back(std::move(port_pair.second));
  return connect_message;
}

}  // namespace cast_api_bindings

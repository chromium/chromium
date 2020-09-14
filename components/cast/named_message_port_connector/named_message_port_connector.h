// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_NAMED_MESSAGE_PORT_CONNECTOR_H_
#define COMPONENTS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_NAMED_MESSAGE_PORT_CONNECTOR_H_

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"

namespace cast_api_bindings {

// Injects an API into |frame| through which it can connect MessagePorts to one
// or more services registered by the caller.
// Platform specific details, such as how the script resources are injected, and
// how the connection message is posted to the page, are delegated to the
// caller.
// TODO(crbug.com/1126571): Migrate off Blink::WebMessagePort to a
// platform-agnostic MessagePort abstraction.
class NamedMessagePortConnector
    : public blink::WebMessagePort::MessageReceiver {
 public:
  // Signature of callback to be invoked when a port is connected.
  // The callback should return true if the connection request was valid.
  using PortConnectedCallback =
      base::RepeatingCallback<bool(base::StringPiece, blink::WebMessagePort)>;

  NamedMessagePortConnector();
  ~NamedMessagePortConnector() override;

  NamedMessagePortConnector(const NamedMessagePortConnector&) = delete;
  NamedMessagePortConnector& operator=(const NamedMessagePortConnector&) =
      delete;

  // Sets the callback which will be invoked when a port is connected.
  void RegisterPortHandler(PortConnectedCallback handler);

  // Returns a connection message which should be posted to the page on
  // every navigation.
  // Calling this method will drop any preexisting connections made to the page.
  blink::WebMessagePort::Message GetConnectMessage();

 private:
  // blink::WebMessagePort::MessageReceiver implementation:
  bool OnMessage(blink::WebMessagePort::Message message) override;

  PortConnectedCallback handler_;
  blink::WebMessagePort control_port_;
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_NAMED_MESSAGE_PORT_CONNECTOR_H_

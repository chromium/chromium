// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_NAMED_MESSAGE_PORT_CONNECTOR_H_
#define COMPONENTS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_NAMED_MESSAGE_PORT_CONNECTOR_H_

#include <string_view>

#include "base/functional/callback.h"
#include "components/cast/message_port/message_port.h"

namespace cast_api_bindings {

// Injects an API into |frame| through which it can connect MessagePorts to one
// or more services registered by the caller.
// Platform specific details, such as how the script resources are injected, and
// how the connection message is posted to the page, are delegated to the
// caller.
class NamedMessagePortConnector : public MessagePort::Receiver {
 public:
  // Signature of callback to be invoked when a port is connected.
  // The callback should return true if the connection request was valid.
  using PortConnectedCallback =
      base::RepeatingCallback<bool(std::string_view,
                                   std::unique_ptr<MessagePort>)>;

  NamedMessagePortConnector();
  ~NamedMessagePortConnector() override;

  NamedMessagePortConnector(const NamedMessagePortConnector&) = delete;
  NamedMessagePortConnector& operator=(const NamedMessagePortConnector&) =
      delete;

  // Sets the callback which will be invoked when a port is connected.
  void RegisterPortHandler(PortConnectedCallback handler);

  // Returns a data payload and MessagePort which, when posted into a web
  // content main frame, will establish a connection between |this| and the
  // NamedMessagePortConnector JavaScript module.
  void GetConnectMessage(std::string* message,
                         std::unique_ptr<MessagePort>* port);

 private:
  // MessagePort::Receiver implementation.
  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<MessagePort>> ports) final;
  void OnPipeError() final;

  PortConnectedCallback handler_;
  std::unique_ptr<MessagePort> control_port_;
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_NAMED_MESSAGE_PORT_CONNECTOR_H_

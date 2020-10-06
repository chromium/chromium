// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_API_BINDINGS_MANAGER_H_
#define COMPONENTS_CAST_API_BINDINGS_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "components/cast/cast_component_export.h"
#include "components/cast/message_port/message_port.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"

namespace cast_api_bindings {

// Allows the caller to specify the JS that should be injected into a container,
// and to register handlers for communication with the content.
class CAST_COMPONENT_EXPORT Manager {
 public:
  // TODO(crbug.com/1135379): Deprecated; remove or redefine after fixing
  // downstream dependencies
  using MessagePortConnectedHandler =
      base::RepeatingCallback<void(blink::WebMessagePort)>;

  using MessagePortProxyConnectedHandler = base::RepeatingCallback<void(
      std::unique_ptr<cast_api_bindings::MessagePort>)>;

  Manager();
  virtual ~Manager();

  // TODO(crbug.com/1135379): Deprecated; remove after fixing downstream
  // dependencies
  void RegisterPortHandler(base::StringPiece port_name,
                           MessagePortConnectedHandler handler);

  // Registers a |handler| which will receive MessagePorts originating from
  // the frame's web content. |port_name| is an alphanumeric string that is
  // consistent across JS and native code.
  // All handlers must be Unregistered() before |this| is destroyed.
  void RegisterPortHandler(base::StringPiece port_name,
                           MessagePortProxyConnectedHandler handler);

  // Unregisters a previously registered handler.
  // The owner of Manager is responsible for ensuring that all
  // handlers are unregistered before |this| is deleted.
  void UnregisterPortHandler(base::StringPiece port_name);

  // Registers a |binding_script| for injection in the frame.
  // Replaces registered bindings with the same |binding_name|.
  virtual void AddBinding(base::StringPiece binding_name,
                          base::StringPiece binding_script) = 0;

 protected:
  // TODO(crbug.com/1135379): Deprecated; remove after fixing downstream
  // dependencies
  bool OnPortConnected(base::StringPiece port_name, blink::WebMessagePort port);

  // Called by platform-specific implementations when the content requests a
  // connection to |port_name|.
  // Returns |false| if the port was invalid or not registered in advance, at
  // which point the matchmaking port should be dropped.
  bool OnPortConnected(base::StringPiece port_name,
                       std::unique_ptr<cast_api_bindings::MessagePort> port);

 private:
  // TODO(crbug.com/1135379): Deprecated; remove after fixing downstream
  // dependencies
  base::flat_map<std::string, MessagePortConnectedHandler> port_handlers_;

  base::flat_map<std::string, MessagePortProxyConnectedHandler>
      port_proxy_handlers_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_API_BINDINGS_MANAGER_H_

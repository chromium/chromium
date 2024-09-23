// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_API_BINDINGS_MANAGER_H_
#define COMPONENTS_CAST_API_BINDINGS_MANAGER_H_

#include <map>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/cast/cast_component_export.h"
#include "components/cast/message_port/message_port.h"

namespace cast_api_bindings {

// Allows the caller to specify the JS that should be injected into a container,
// and to register handlers for communication with the content.
class CAST_COMPONENT_EXPORT Manager {
 public:
  using MessagePortConnectedHandler = base::RepeatingCallback<void(
      std::unique_ptr<cast_api_bindings::MessagePort>)>;

  Manager();

  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;

  virtual ~Manager();

  // Registers a |handler| which will receive MessagePorts originating from
  // the frame's web content. |port_name| is an alphanumeric string that is
  // consistent across JS and native code.
  // All handlers must be Unregistered() before |this| is destroyed.
  void RegisterPortHandler(std::string_view port_name,
                           MessagePortConnectedHandler handler);

  // Unregisters a previously registered handler.
  // The owner of Manager is responsible for ensuring that all
  // handlers are unregistered before |this| is deleted.
  void UnregisterPortHandler(std::string_view port_name);

  // Registers a |binding_script| for injection in the frame.
  // Replaces registered bindings with the same |binding_name|.
  virtual void AddBinding(std::string_view binding_name,
                          std::string_view binding_script) = 0;

 protected:
  // Called by platform-specific implementations when the content requests a
  // connection to |port_name|.
  // Returns |false| if the port was invalid or not registered in advance, at
  // which point the matchmaking port should be dropped.
  bool OnPortConnected(std::string_view port_name,
                       std::unique_ptr<cast_api_bindings::MessagePort> port);

 private:
  base::flat_map<std::string, MessagePortConnectedHandler> port_handlers_;
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_API_BINDINGS_MANAGER_H_

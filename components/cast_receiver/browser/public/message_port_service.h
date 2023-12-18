// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_MESSAGE_PORT_SERVICE_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_MESSAGE_PORT_SERVICE_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"

namespace cast_api_bindings {
class MessagePort;
}  // namespace cast_api_bindings

namespace cast_receiver {

// This class defines a wrapper around MessagePort functionality to handle
// communicating with message ports, as well as their registration.
class MessagePortService {
 public:
  using CreatePairCallback = base::RepeatingCallback<void(
      std::unique_ptr<cast_api_bindings::MessagePort>*,
      std::unique_ptr<cast_api_bindings::MessagePort>*)>;

  virtual ~MessagePortService() = default;

  // Connects |port| to the remote port with name |port_name| asynchronously.
  virtual void ConnectToPortAsync(
      std::string_view port_name,
      std::unique_ptr<cast_api_bindings::MessagePort> port) = 0;

  // Registers a port opened locally via a port transfer. This allocates a new
  // |channel_id| for the port, which is returned by the function.
  virtual uint32_t RegisterOutgoingPort(
      std::unique_ptr<cast_api_bindings::MessagePort> port) = 0;

  // Registers a port opened by the peer via a port transfer. |channel_id| is
  // provided by the peer.
  virtual void RegisterIncomingPort(
      uint32_t channel_id,
      std::unique_ptr<cast_api_bindings::MessagePort> port) = 0;

  // Removes the handler for |channel_id|. Note that this will destroy it. May
  // only be called on a valid |channel_id| already assocaited with a previously
  // registered |port|.
  virtual void Remove(uint32_t channel_id) = 0;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_MESSAGE_PORT_SERVICE_H_

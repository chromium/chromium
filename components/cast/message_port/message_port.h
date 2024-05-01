// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_MESSAGE_PORT_MESSAGE_PORT_H_
#define COMPONENTS_CAST_MESSAGE_PORT_MESSAGE_PORT_H_

#include <memory>
#include <string_view>
#include <vector>

#include "components/cast/cast_component_export.h"

namespace cast_api_bindings {

// HTML5 MessagePort abstraction; allows usage of the platform MessagePort type
// without exposing details of the message format, paired port creation, or
// transfer of ports.
class CAST_COMPONENT_EXPORT MessagePort {
 public:
  // Implemented by receivers of messages from the MessagePort class.
  class Receiver {
   public:
    virtual ~Receiver();

    // Receives a |message| and ownership of |ports|.
    virtual bool OnMessage(std::string_view message,
                           std::vector<std::unique_ptr<MessagePort>> ports) = 0;

    // Receives an error.
    virtual void OnPipeError() = 0;
  };

  virtual ~MessagePort();

  // Sends a |message| from the port.
  virtual bool PostMessage(std::string_view message) = 0;

  // Sends a |message| from the port along with transferable |ports|.
  virtual bool PostMessageWithTransferables(
      std::string_view message,
      std::vector<std::unique_ptr<MessagePort>> ports) = 0;

  // Sets the |receiver| for messages arriving to this port. May only be set
  // once.
  virtual void SetReceiver(
      cast_api_bindings::MessagePort::Receiver* receiver) = 0;

  // Closes the underlying port.
  virtual void Close() = 0;

  // Whether a message can be posted; may be used to check the state of the port
  // without posting a message.
  virtual bool CanPostMessage() const = 0;
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_MESSAGE_PORT_MESSAGE_PORT_H_

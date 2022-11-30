// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "components/cast_receiver/common/public/status.h"
#include "third_party/cast_core/public/src/proto/web/message_channel.pb.h"

namespace cast_api_bindings {
class MessagePort;
}  // namespace cast_api_bindings

namespace chromecast {

// This class defines a wrapper around MessagePort functionality to handle
// communicating with message ports, as well as their registration.
class MessagePortService {
 public:
  using CreatePairCallback = base::RepeatingCallback<void(
      std::unique_ptr<cast_api_bindings::MessagePort>*,
      std::unique_ptr<cast_api_bindings::MessagePort>*)>;

  virtual ~MessagePortService() = default;

  // Handles a message incoming over RPC. The message will be routed to the
  // appropriate destination based on its channel ID. Returns |true| in the case
  // that this message was successfully processed, and false in all other cases
  // including the case that there's no handler for the incoming channel ID.
  virtual cast_receiver::Status HandleMessage(cast::web::Message message) = 0;

  // Connects |port| to the remote port with name |port_name| asynchronously.
  virtual void ConnectToPortAsync(
      base::StringPiece port_name,
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

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_MESSAGE_PORT_SERVICE_H_

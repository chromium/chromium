// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_MESSAGE_CONNECTOR_H_
#define COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_MESSAGE_CONNECTOR_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"

namespace cast_api_bindings {

struct Message;

// Represents one end of a channel, typically used by MessagePort.
class MessageConnector {
 public:
  // Accepts a |message| from another connector.
  // Returns |true| if accepted by the connector; else, |false|.
  virtual bool Accept(Message message) = 0;

  // Accepts a |result| from another connector, typically after calling Accept.
  // Returns |true| if accepted by the connector; else, |false|.
  virtual bool AcceptResult(bool result) = 0;

  // Called when peer is ready to process calls to Accept.
  virtual void OnPeerStarted() = 0;

  // Called when a peer has an error or is detached.
  virtual void OnPeerError() = 0;

  // Set this connector's peer to |other|, which must be on the same channel.
  void SetPeer(MessageConnector* other);

  // Detaches from |peer_|. Typically called by |peer_| to notify this
  // connector of its invalidation.
  void DetachPeer();

  // Starts this connector; indicates this connector is ready for Accept.
  void Start();

  // Retrieve the |channel_id_| for this connector.
  uint32_t channel_id() const { return channel_id_; }

  // Whether the connector has |started_|
  bool started() const { return started_; }

 protected:
  MessageConnector();
  explicit MessageConnector(uint32_t channel_id);
  virtual ~MessageConnector();

  uint32_t channel_id_;
  raw_ptr<MessageConnector> peer_ = nullptr;
  bool started_ = false;
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_MESSAGE_PORT_CAST_CORE_MESSAGE_CONNECTOR_H_

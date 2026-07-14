// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_MESSAGE_STREAM_CLIENT_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_MESSAGE_STREAM_CLIENT_H_

#include <string>

#include "base/observer_list_types.h"

namespace browser_actuator {

// A client of a server→client message stream: a long-lived connection
// that delivers discrete, ordered messages — serialized protos, parsed by
// the layer that knows their type — to observers. Implementations own the
// underlying connection and its lifecycle: reconnects, keep-alives, and
// failure policy are not the consumer's concern.
//
// The interface is wire-format-agnostic; the messages and the stream
// status mean the same thing however the stream is encoded on the wire.
//
// All methods must be called on the owning sequence. Observers may call
// Disconnect() or Connect() from OnStreamMessage(), but must not destroy
// the client synchronously from a notification.
class MessageStreamClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // A complete message was received: one serialized proto, ready for
    // protobuf-lite parsing by the layer that knows its type.
    virtual void OnStreamMessage(const std::string& message) = 0;

    // The RPC finished: `status` is a serialized google.rpc.Status. The
    // client will stop (no automatic reconnect) once the connection
    // closes.
    virtual void OnStreamStatus(const std::string& status) {}

    // The client established a valid stream (true) or lost it (false). A
    // `false` notification covers scheduled reconnects, permanent
    // failures, and explicit Disconnect() calls alike.
    virtual void OnStreamConnectionStateChange(bool connected) {}
  };

  virtual ~MessageStreamClient() = default;

  // Starts the connection. No-op if already connected, connecting, or
  // waiting to reconnect. Calling Connect() after a permanent failure or
  // a terminal status starts over.
  virtual void Connect() = 0;

  // Closes the connection and cancels any pending reconnect.
  virtual void Disconnect() = 0;

  // True while a valid stream is open.
  virtual bool IsConnected() const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_MESSAGE_STREAM_CLIENT_H_

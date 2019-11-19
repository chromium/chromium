// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_CONNECTION_OBSERVER_H_
#define COMPONENTS_ARC_SESSION_CONNECTION_OBSERVER_H_

namespace arc {
namespace internal {

// Observer to listen events for connection.
class ConnectionObserverBase {
 public:
  // Called once the connection is ready.
  // TODO(hidehiko): Currently, this means Instance is ready.
  // Later, this will be called when Instance::Init() is completed
  // for ARC full-duplex mojo connection.
  virtual void OnConnectionReady() {}

  // Called once the connection is closed.
  // Currently, this means Instance is closed.
  virtual void OnConnectionClosed() {}

 protected:
  virtual ~ConnectionObserverBase() = default;
};

}  // namespace internal

// Observer to listen events for connection for specific type.
// This is for type safeness to prevent listening events of unexpected mojo
// connection.
template <typename InstanceType>
class ConnectionObserver : public internal::ConnectionObserverBase {};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_CONNECTION_OBSERVER_H_

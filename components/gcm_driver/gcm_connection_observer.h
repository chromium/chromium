// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_CONNECTION_OBSERVER_H_
#define COMPONENTS_GCM_DRIVER_GCM_CONNECTION_OBSERVER_H_


namespace net {
class IPEndPoint;
}

namespace gcm {

// Interface for objects observing GCM connection events.
class GCMConnectionObserver {
 public:
  GCMConnectionObserver();
  virtual ~GCMConnectionObserver();

  // Called when a new connection is established and a successful handshake
  // has been performed. Note that |ip_endpoint| is only set if available for
  // the current platform.
  // Default implementation does nothing.
  virtual void OnConnected(const net::IPEndPoint& ip_endpoint);

  // Called when the connection is interrupted.
  // Default implementation does nothing.
  virtual void OnDisconnected();
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_CONNECTION_OBSERVER_H_

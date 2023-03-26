// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SERVER_WAYLAND_SERVER_HANDLE_H_
#define COMPONENTS_EXO_SERVER_WAYLAND_SERVER_HANDLE_H_

namespace exo {

class WaylandServerController;

// This is an opaque type used to represent the lifetime of the wayland server.
// Clients should delete this object when they want exo to remove the server.
class WaylandServerHandle {
 public:
  // Handles can not be moved or assigned
  WaylandServerHandle(WaylandServerHandle&&) = delete;
  WaylandServerHandle(const WaylandServerHandle&) = delete;
  WaylandServerHandle& operator=(WaylandServerHandle&&) = delete;
  WaylandServerHandle& operator=(const WaylandServerHandle&) = delete;

  ~WaylandServerHandle();

 private:
  // Only the controller can make these.
  friend class WaylandServerController;
  WaylandServerHandle();
};

}  // namespace exo
#endif  // COMPONENTS_EXO_SERVER_WAYLAND_SERVER_HANDLE_H_

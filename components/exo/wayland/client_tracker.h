// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENT_TRACKER_H_
#define COMPONENTS_EXO_WAYLAND_CLIENT_TRACKER_H_

#include <memory>

#include <wayland-server-protocol-core.h>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"

struct wl_client;
struct wl_display;

namespace exo::wayland {

// Utility class to keep track of the lifetime of wl_client. This is necessary
// as resources owned by wl_client are freed in the process of destruction, and
// accessing client resources should not happen after this time (see
// crbug.com/1433187). All resources and associated user data are destroyed and
// freed before the wl_client is itself deleted.
class ClientTracker {
 public:
  explicit ClientTracker(wl_display* wl_display);
  virtual ~ClientTracker();

  // Returns true if `client` has begun destruction. Client resources should not
  // be queried after this point.
  bool IsClientDestroyed(wl_client* client) const;

  int NumClientsTrackedForTesting() const;

 private:
  // Listener called when the wl_client has been successfully created for
  // `wl_display_` and immediately when the clients begins destruction (before
  // associated resources have been freed).
  struct ClientListener {
    ClientListener(ClientTracker* tracker, wl_notify_func_t notify);
    wl_listener listener;
    raw_ptr<ClientTracker> tracker;
  };

  // Called when a wl_client has successfully been created for `wl_display_`.
  static void OnClientCreated(struct wl_listener* listener, void* data);

  // Called when a wl_client associated with the listener begins destruction.
  static void OnClientDestroyed(struct wl_listener* listener, void* data);

  // Handles the `OnClientCreated()` event for clients associated with this
  // tracker instance.
  void HandleClientCreated(wl_client* client);

  // Handles the `OnClientDestroyed()` event for clients associated with this
  // tracker instance.
  void HandleClientDestroyed(wl_client* client);

  raw_ptr<wl_display> wl_display_;

  ClientListener client_created_listener_;

  // Presence in `client_destroyed_listeners_` indicates the client has been
  // created and has yet to be destroyed.
  base::flat_map<wl_client*, std::unique_ptr<ClientListener>>
      client_destroyed_listeners_;
};

}  // namespace exo::wayland

#endif  // COMPONENTS_EXO_WAYLAND_CLIENT_TRACKER_H_

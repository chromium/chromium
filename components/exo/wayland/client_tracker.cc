// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/client_tracker.h"

namespace exo::wayland {

ClientTracker::ClientTracker(wl_display* wl_display)
    : wl_display_(wl_display),
      client_created_listener_(this, &ClientTracker::OnClientCreated) {
  wl_display_add_client_created_listener(wl_display,
                                         &client_created_listener_.listener);
}

ClientTracker::~ClientTracker() {
  while (client_destroyed_listeners_.size() > 0) {
    auto it = client_destroyed_listeners_.begin();
    wl_list_remove(&it->second->listener.link);
    client_destroyed_listeners_.erase(it->first);
  }
  // Remove the listener from the display's destroy signal.
  wl_list_remove(&client_created_listener_.listener.link);
}

bool ClientTracker::IsClientDestroyed(wl_client* client) const {
  return client_destroyed_listeners_.find(client) ==
         client_destroyed_listeners_.end();
}

int ClientTracker::NumClientsTrackedForTesting() const {
  return client_destroyed_listeners_.size();
}

ClientTracker::ClientListener::ClientListener(ClientTracker* tracker,
                                              wl_notify_func_t notify)
    : tracker(tracker) {
  listener.notify = notify;
}

// static.
void ClientTracker::OnClientCreated(struct wl_listener* listener, void* data) {
  ClientListener* client_created_listener = wl_container_of(
      listener, /*sample=*/client_created_listener, /*member=*/listener);
  wl_client* client = static_cast<wl_client*>(data);
  ClientTracker* tracker = client_created_listener->tracker;
  tracker->HandleClientCreated(client);
}

// static.
void ClientTracker::OnClientDestroyed(struct wl_listener* listener,
                                      void* data) {
  ClientListener* client_destroyed_listener = wl_container_of(
      listener, /*sample=*/client_destroyed_listener, /*member=*/listener);
  wl_client* client = static_cast<wl_client*>(data);
  ClientTracker* tracker = client_destroyed_listener->tracker;
  tracker->HandleClientDestroyed(client);
}

void ClientTracker::HandleClientCreated(wl_client* client) {
  // Set up the destruction listener for the newly created client.
  client_destroyed_listeners_.emplace(
      client, std::make_unique<ClientListener>(
                  this, &ClientTracker::OnClientDestroyed));
  auto& client_destroyed_listener = client_destroyed_listeners_.at(client);
  wl_client_add_destroy_listener(client, &client_destroyed_listener->listener);
}

void ClientTracker::HandleClientDestroyed(wl_client* client) {
  // Remove the listener from the client's destroy signal.
  auto& client_destroyed_listener = client_destroyed_listeners_.at(client);
  wl_list_remove(&client_destroyed_listener->listener.link);
  client_destroyed_listeners_.erase(client);
}

}  // namespace exo::wayland

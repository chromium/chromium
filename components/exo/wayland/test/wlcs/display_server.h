// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_WLCS_DISPLAY_SERVER_H_
#define COMPONENTS_EXO_WAYLAND_TEST_WLCS_DISPLAY_SERVER_H_

#include <memory>

#include "components/exo/wayland/test/wlcs/wlcs_helpers.h"
#include "third_party/wlcs/src/include/wlcs/display_server.h"

struct wl_display;
struct wl_surface;

namespace exo::wlcs {

// Used by WLCS during tests to control the WL server, and to create
// windows/pointers/touch handlers.
class DisplayServer : public WlcsDisplayServer {
 public:
  DisplayServer();

  // This object is not movable or copyable.
  DisplayServer(const DisplayServer&) = delete;
  DisplayServer& operator=(const DisplayServer&) = delete;

  ~DisplayServer();

  void Start();
  void Stop();
  int CreateSocket();
  void PositionWindow(wl_display* client, wl_surface* surface, int x, int y);

  ScopedWlcsServer* server() { return server_.get(); }

 private:
  std::unique_ptr<ScopedWlcsServer> server_;
};

}  // namespace exo::wlcs

#endif  // COMPONENTS_EXO_WAYLAND_TEST_WLCS_DISPLAY_SERVER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXO_WAYLAND_SERVER_CONTROLLER_H_
#define CHROMECAST_BROWSER_EXO_WAYLAND_SERVER_CONTROLLER_H_

#include <memory>

namespace exo {
class Display;
class WMHelper;

namespace wayland {
class Server;
class WaylandWatcher;
}  // namespace wayland
}  // namespace exo

namespace chromecast {

class CastWindowManagerAura;

class WaylandServerController {
 public:
  explicit WaylandServerController(CastWindowManagerAura* window_manager);

  WaylandServerController(const WaylandServerController&) = delete;
  WaylandServerController& operator=(const WaylandServerController&) = delete;

  ~WaylandServerController();

 private:
  std::unique_ptr<exo::WMHelper> wm_helper_;
  std::unique_ptr<exo::Display> display_;
  std::unique_ptr<exo::wayland::Server> wayland_server_;
  std::unique_ptr<exo::wayland::WaylandWatcher> wayland_watcher_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_EXO_WAYLAND_SERVER_CONTROLLER_H_

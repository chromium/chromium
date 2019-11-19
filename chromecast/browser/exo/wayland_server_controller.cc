// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/exo/wayland_server_controller.h"

#include "chromecast/browser/exo/cast_wm_helper.h"
#include "chromecast/graphics/cast_screen.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/wayland_watcher.h"
#include "components/exo/wm_helper.h"

namespace chromecast {

WaylandServerController::WaylandServerController(
    CastWindowManagerAura* window_manager) {
  wm_helper_ = std::make_unique<exo::CastWMHelper>(
      window_manager, static_cast<CastScreen*>(CastScreen::GetScreen()));
  exo::WMHelper::SetInstance(wm_helper_.get());
  display_ = std::make_unique<exo::Display>();
  wayland_server_ = exo::wayland::Server::Create(display_.get());
  // Wayland server creation can fail if XDG_RUNTIME_DIR is not set correctly.
  if (wayland_server_) {
    wayland_watcher_ =
        std::make_unique<exo::wayland::WaylandWatcher>(wayland_server_.get());
  } else {
    LOG(ERROR) << "Wayland server creation failed";
  }
}

WaylandServerController::~WaylandServerController() {
  wayland_watcher_.reset();
  wayland_server_.reset();
  display_.reset();
  exo::WMHelper::SetInstance(nullptr);
  wm_helper_.reset();
}

}  // namespace chromecast

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/server/wayland_server_handle.h"

#include "components/exo/server/wayland_server_controller.h"

namespace exo {

WaylandServerHandle::~WaylandServerHandle() {
  WaylandServerController* controller = WaylandServerController::Get();
  if (!controller) {
    return;
  }
  controller->CloseSocket(this);
}

WaylandServerHandle::WaylandServerHandle() = default;

}  // namespace exo

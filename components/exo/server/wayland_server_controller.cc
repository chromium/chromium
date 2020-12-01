// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/server/wayland_server_controller.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/task/current_thread.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/display.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/toast_surface_manager.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/wayland_watcher.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"

namespace exo {

// static
std::unique_ptr<WaylandServerController>
WaylandServerController::CreateIfNecessary(
    std::unique_ptr<DataExchangeDelegate> data_exchange_delegate,
    std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
    std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
    std::unique_ptr<ToastSurfaceManager> toast_surface_manager) {
  return std::make_unique<WaylandServerController>(
      std::move(data_exchange_delegate),
      std::move(notification_surface_manager),
      std::move(input_method_surface_manager),
      std::move(toast_surface_manager));
}

WaylandServerController::~WaylandServerController() {
}

WaylandServerController::WaylandServerController(
    std::unique_ptr<DataExchangeDelegate> data_exchange_delegate,
    std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
    std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
    std::unique_ptr<ToastSurfaceManager> toast_surface_manager)
    : wm_helper_(std::make_unique<WMHelperChromeOS>()),
      display_(
          std::make_unique<Display>(std::move(notification_surface_manager),
                                    std::move(input_method_surface_manager),
                                    std::move(toast_surface_manager),
                                    std::move(data_exchange_delegate))),

      wayland_server_(wayland::Server::Create(display_.get())) {
  // Wayland server creation can fail if XDG_RUNTIME_DIR is not set correctly.
  if (wayland_server_) {
    wayland_watcher_ =
        std::make_unique<wayland::WaylandWatcher>(wayland_server_.get());
  }
}

}  // namespace exo

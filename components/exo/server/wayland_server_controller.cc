// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/server/wayland_server_controller.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/task/current_thread.h"
#include "components/exo/capabilities.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/display.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/toast_surface_manager.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"

namespace exo {
namespace {

// Directory name where all custom wayland sockets will live.
constexpr base::FilePath::CharType kCustomServerDir[] =
    FILE_PATH_LITERAL("wayland");

}  // namespace

// Custom wayland sockets are stored at:
//
//              /<sibling>/wayland/<context>/<unique>/<socket>
//
// where:
//  - "sibling" is a sibling of the $XDG_RUNTIME_DIR
//  - "context" is a directory to be bind-mounted into whatever namespace needs
//    access to the wayland server.
//  - "unique" is a directory created to prevent collisions of servers in the
//    same context.
//  - "socket" is the name of the wayland socket, usually "wayland-0"
// This is documented in go/secure-exo-ids
std::unique_ptr<WaylandServerController::PathHelper>
WaylandServerController::PathHelper::Create(const Capabilities& capabilities) {
  char* xdg_dir_str = getenv("XDG_RUNTIME_DIR");
  if (!xdg_dir_str) {
    LOG(ERROR) << "XDG_RUNTIME_DIR is not set.";
    return nullptr;
  }
  std::string security_context = capabilities.GetSecurityContext();
  if (security_context.empty()) {
    LOG(ERROR) << "Providing an empty security context is an error.";
    return nullptr;
  }
  base::ScopedTempDir dir;
  base::FilePath parent_path = base::FilePath(xdg_dir_str)
                                   .DirName()
                                   .Append(kCustomServerDir)
                                   .Append(security_context);
  if (!dir.CreateUniqueTempDirUnderPath(parent_path)) {
    LOG(ERROR) << "Unable to create runtime directory under " << parent_path;
    return nullptr;
  }
  return base::WrapUnique(
      new WaylandServerController::PathHelper(std::move(dir)));
}

WaylandServerController::PathHelper::PathHelper(base::ScopedTempDir runtime_dir)
    : runtime_dir_(std::move(runtime_dir)),
      socket_path_(
          runtime_dir_.GetPath().Append(wayland::Server::GetSocketName())) {}

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

WaylandServerController::~WaylandServerController() {}

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
      wayland_server_(wayland::Server::Create(display_.get())) {}

}  // namespace exo

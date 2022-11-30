// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/server/wayland_server_controller.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/task/current_thread.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/display.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/security_delegate.h"
#include "components/exo/toast_surface_manager.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"

namespace exo {

namespace {
WaylandServerController* g_instance = nullptr;
}

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

// static
WaylandServerController* WaylandServerController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

WaylandServerController::~WaylandServerController() {
  // TODO(https://crbug.com/1124106): Investigate if we can eliminate Shutdown
  // methods.
  display_->Shutdown();
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
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
                                    std::move(data_exchange_delegate))) {
  DCHECK(!g_instance);
  g_instance = this;
  CreateServer(
      /*security_delegate=*/nullptr,
      base::BindOnce([](bool success, const base::FilePath& path) {
        DCHECK(success) << "Failed to start the default wayland server.";
      }));
}

void WaylandServerController::CreateServer(
    std::unique_ptr<SecurityDelegate> security_delegate,
    wayland::Server::StartCallback callback) {
  bool async = true;
  if (!security_delegate) {
    security_delegate = SecurityDelegate::GetDefaultSecurityDelegate();
    async = false;
  }

  std::unique_ptr<wayland::Server> server =
      wayland::Server::Create(display_.get(), std::move(security_delegate));
  auto* server_ptr = server.get();
  auto start_callback = base::BindOnce(&WaylandServerController::OnStarted,
                                       weak_factory_.GetWeakPtr(),
                                       std::move(server), std::move(callback));

  if (async) {
    server_ptr->StartAsync(std::move(start_callback));
  } else {
    server_ptr->StartWithDefaultPath(std::move(start_callback));
  }
}

void WaylandServerController::OnStarted(std::unique_ptr<wayland::Server> server,
                                        wayland::Server::StartCallback callback,
                                        bool success,
                                        const base::FilePath& path) {
  if (success) {
    DCHECK(server->socket_path() == path);
    auto iter_success_pair = servers_.emplace(path, std::move(server));
    DCHECK(iter_success_pair.second);
  }
  std::move(callback).Run(success, path);
}

void WaylandServerController::DeleteServer(const base::FilePath& path) {
  DCHECK(servers_.contains(path));
  wayland::Server::DestroyAsync(std::move(servers_.at(path)));
  servers_.erase(path);
}

}  // namespace exo

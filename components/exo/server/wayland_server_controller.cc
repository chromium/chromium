// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/server/wayland_server_controller.h"

#include <memory>

#include "base/atomic_sequence_num.h"
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
#include "components/exo/server/wayland_server_handle.h"
#include "components/exo/toast_surface_manager.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wm_helper.h"

namespace exo {

namespace {
WaylandServerController* g_instance = nullptr;
}  // namespace

// static
std::unique_ptr<WaylandServerController>
WaylandServerController::CreateIfNecessary(
    std::unique_ptr<DataExchangeDelegate> data_exchange_delegate,
    std::unique_ptr<SecurityDelegate> security_delegate,
    std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
    std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
    std::unique_ptr<ToastSurfaceManager> toast_surface_manager) {
  return std::make_unique<WaylandServerController>(
      std::move(data_exchange_delegate), std::move(security_delegate),
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
  // TODO(crbug.com/40717074): Investigate if we can eliminate Shutdown
  // methods.
  display_->Shutdown();
  wayland::Server::SetServerGetter(base::NullCallback());
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

wayland::Server* WaylandServerController::GetServerForDisplay(
    wl_display* display) {
  if (default_server_ && default_server_->GetWaylandDisplay() == display) {
    return default_server_.get();
  }

  for (const auto& pair : on_demand_servers_) {
    if (pair.second->GetWaylandDisplay() == display) {
      return pair.second.get();
    }
  }

  return nullptr;
}

WaylandServerController::WaylandServerController(
    std::unique_ptr<DataExchangeDelegate> data_exchange_delegate,
    std::unique_ptr<SecurityDelegate> security_delegate,
    std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
    std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
    std::unique_ptr<ToastSurfaceManager> toast_surface_manager)
    : wm_helper_(std::make_unique<WMHelper>()),
      display_(
          std::make_unique<Display>(std::move(notification_surface_manager),
                                    std::move(input_method_surface_manager),
                                    std::move(toast_surface_manager),
                                    std::move(data_exchange_delegate))) {
  DCHECK(!g_instance);
  g_instance = this;
  default_server_ =
      wayland::Server::Create(display_.get(), std::move(security_delegate));
  default_server_->StartWithDefaultPath(base::BindOnce([](bool success) {
    DCHECK(success) << "Failed to start the default wayland server.";
  }));
  wayland::Server::SetServerGetter(base::BindRepeating(
      &WaylandServerController::GetServerForDisplay, base::Unretained(this)));
}

void WaylandServerController::ListenOnSocket(
    std::unique_ptr<SecurityDelegate> security_delegate,
    base::ScopedFD socket,
    base::OnceCallback<void(std::unique_ptr<WaylandServerHandle>)> callback) {
  std::unique_ptr<wayland::Server> server =
      wayland::Server::Create(display_.get(), std::move(security_delegate));
  auto* server_ptr = server.get();
  auto start_callback = base::BindOnce(&WaylandServerController::OnSocketAdded,
                                       weak_factory_.GetWeakPtr(),
                                       std::move(server), std::move(callback));
  server_ptr->StartWithFdAsync(std::move(socket), std::move(start_callback));
}

void WaylandServerController::OnSocketAdded(
    std::unique_ptr<wayland::Server> server,
    base::OnceCallback<void(std::unique_ptr<WaylandServerHandle>)> callback,
    bool success) {
  if (!success) {
    std::move(callback).Run(nullptr);
    return;
  }

  // WrapUnique() is needed since the constructor is private.
  auto handle = base::WrapUnique(new WaylandServerHandle());
  on_demand_servers_.emplace(handle.get(), std::move(server));
  std::move(callback).Run(std::move(handle));
}

void WaylandServerController::CloseSocket(WaylandServerHandle* server) {
  on_demand_servers_.erase(server);
}

}  // namespace exo

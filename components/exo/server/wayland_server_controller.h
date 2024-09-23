// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SERVER_WAYLAND_SERVER_CONTROLLER_H_
#define COMPONENTS_EXO_SERVER_WAYLAND_SERVER_CONTROLLER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/display.h"
#include "components/exo/security_delegate.h"
#include "components/exo/wayland/server.h"

struct wl_display;

namespace exo {

namespace wayland {
class Server;
}  // namespace wayland

class DataExchangeDelegate;
class InputMethodSurfaceManager;
class NotificationSurfaceManager;
class ToastSurfaceManager;
class WMHelper;
class WaylandServerHandle;

class WaylandServerController {
 public:
  static std::unique_ptr<WaylandServerController> CreateForArcIfNecessary(
      std::unique_ptr<DataExchangeDelegate> data_exchange_delegate,
      std::unique_ptr<SecurityDelegate> security_delegate);

  // Creates WaylandServerController. Returns null if controller should not be
  // created.
  static std::unique_ptr<WaylandServerController> CreateIfNecessary(
      std::unique_ptr<DataExchangeDelegate> data_exchange_delegate,
      std::unique_ptr<SecurityDelegate> security_delegate,
      std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
      std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
      std::unique_ptr<ToastSurfaceManager> toast_surface_manager);

  // Returns a handle to the global-singletone instance of the server
  // controller.
  static WaylandServerController* Get();

  WaylandServerController(const WaylandServerController&) = delete;
  WaylandServerController& operator=(const WaylandServerController&) = delete;

  ~WaylandServerController();

  // Gets the Server instance for the `display` if it exists.
  wayland::Server* GetServerForDisplay(wl_display* display);

  InputMethodSurfaceManager* input_method_surface_manager() {
    return display_->input_method_surface_manager();
  }

  WaylandServerController(
      std::unique_ptr<DataExchangeDelegate> data_exchange_delegate,
      std::unique_ptr<SecurityDelegate> security_delegate,
      std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
      std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager,
      std::unique_ptr<ToastSurfaceManager> toast_surface_manager);

  // Creates a wayland server from the given |socket|, with the privileges of
  // the |security_delegate|. Invokes |callback| with a handle to a wayland
  // server on success, nullptr on failure.
  void ListenOnSocket(
      std::unique_ptr<SecurityDelegate> security_delegate,
      base::ScopedFD socket,
      base::OnceCallback<void(std::unique_ptr<WaylandServerHandle>)> callback);

 private:
  void OnSocketAdded(
      std::unique_ptr<wayland::Server> server,
      base::OnceCallback<void(std::unique_ptr<WaylandServerHandle>)> callback,
      bool success);

  // Removes the wayland server that was created by ListenOnSocket() which
  // returned the given |handle|.
  friend class WaylandServerHandle;
  void CloseSocket(WaylandServerHandle* handle);

  std::unique_ptr<WMHelper> wm_helper_;
  std::unique_ptr<Display> display_;
  std::unique_ptr<wayland::Server> default_server_;
  base::flat_map<WaylandServerHandle*, std::unique_ptr<wayland::Server>>
      on_demand_servers_;
  base::WeakPtrFactory<WaylandServerController> weak_factory_{this};
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SERVER_WAYLAND_SERVER_CONTROLLER_H_

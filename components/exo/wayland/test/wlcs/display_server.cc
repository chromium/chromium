// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/wlcs/display_server.h"

#include <memory>

#include "base/logging.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/wayland/fuzzer/server_environment.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/test/wlcs/pointer.h"
#include "components/exo/wayland/test/wlcs/touch.h"
#include "components/exo/wayland/test/wlcs/wlcs_helpers.h"

namespace exo::wlcs {
namespace {

// TODO(b/271365026): This set of protocols was generated Q1 2023, find a way to
// generate it automatically.
WlcsExtensionDescriptor extensions[] = {
    {.name = "wl_compositor", .version = 3},
    {.name = "wl_shm", .version = 1},
    {.name = "zwp_linux_dmabuf_v1", .version = 2},
    {.name = "wl_subcompositor", .version = 1},
    {.name = "wl_output", .version = 3},
    {.name = "wl_data_device_manager", .version = 3},
    {.name = "surface_augmenter", .version = 5},
    {.name = "overlay_prioritizer", .version = 1},
    {.name = "wp_viewporter", .version = 1},
    {.name = "wp_presentation", .version = 1},
    {.name = "wl_seat", .version = 6},
    {.name = "wl_shell", .version = 1},
    {.name = "wp_content_type_manager_v1", .version = 1},
    {.name = "zwp_input_timestamps_manager_v1", .version = 1},
    {.name = "zwp_pointer_gestures_v1", .version = 1},
    {.name = "zwp_pointer_constraints_v1", .version = 1},
    {.name = "zwp_relative_pointer_manager_v1", .version = 1},
    {.name = "zxdg_decoration_manager_v1", .version = 1},
    {.name = "zxdg_output_manager_v1", .version = 3},
    {.name = "zwp_idle_inhibit_manager_v1", .version = 1},
    {.name = "zwp_keyboard_shortcuts_inhibit_manager_v1", .version = 1},
    {.name = "zwp_text_input_manager_v1", .version = 1},
    {.name = "zxdg_shell_v6", .version = 1},
    {.name = "xdg_wm_base", .version = 3},
};

WlcsIntegrationDescriptor descriptor{
    .version = 1,
    .num_extensions = sizeof(extensions) / sizeof(WlcsExtensionDescriptor),
    .supported_extensions = extensions,
};

}  // namespace

DisplayServer::DisplayServer() {
  WlcsDisplayServer::version = 2;
  WlcsDisplayServer::start = [](WlcsDisplayServer* server) {
    static_cast<DisplayServer*>(server)->Start();
  };
  WlcsDisplayServer::stop = [](WlcsDisplayServer* server) {
    static_cast<DisplayServer*>(server)->Stop();
  };
  WlcsDisplayServer::create_client_socket = [](WlcsDisplayServer* server) {
    return static_cast<DisplayServer*>(server)->CreateSocket();
  };
  WlcsDisplayServer::position_window_absolute = [](WlcsDisplayServer* server,
                                                   wl_display* client,
                                                   wl_surface* surface, int x,
                                                   int y) {
    static_cast<DisplayServer*>(server)->PositionWindow(client, surface, x, y);
  };
  WlcsDisplayServer::create_pointer =
      [](WlcsDisplayServer* server) -> WlcsPointer* {
    return new Pointer(static_cast<DisplayServer*>(server));
  };
  WlcsDisplayServer::create_touch =
      [](WlcsDisplayServer* server) -> WlcsTouch* {
    return new Touch(static_cast<DisplayServer*>(server));
  };
  WlcsDisplayServer::get_descriptor =
      [](WlcsDisplayServer const* server) -> const WlcsIntegrationDescriptor* {
    return &descriptor;
  };
}

DisplayServer::~DisplayServer() = default;

void DisplayServer::Start() {
  server_ = std::make_unique<ScopedWlcsServer>();
}

void DisplayServer::Stop() {
  server_.reset();
}

int DisplayServer::CreateSocket() {
  return server_->AddClient();
}

void DisplayServer::PositionWindow(wl_display* client,
                                   wl_surface* surface,
                                   int x,
                                   int y) {
  wl_resource* surface_res = server_->ObjectToResource(client, surface);
  Surface* exo_surface = wayland::GetUserDataAs<Surface>(surface_res);
  // Ensure surface is toplevel (i.e. the root of its window hierarchy).
  CHECK(GetShellRootSurface(exo_surface->window()->GetToplevelWindow()) ==
        exo_surface);
  ShellSurfaceBase* ssb =
      static_cast<ShellSurfaceBase*>(exo_surface->GetDelegateForTesting());
  WlcsEnvironment::Get().env.RunOnUiThreadBlocking(base::BindOnce(
      [](ShellSurfaceBase* ssb, int x, int y) {
        const auto& bounds = ssb->GetWidget()->GetWindowBoundsInScreen();
        ssb->SetWindowBounds(gfx::Rect{x, y, bounds.width(), bounds.height()});
      },
      ssb, x, y));
}

}  // namespace exo::wlcs

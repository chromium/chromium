// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_ui_controls.h"

#include <stdint.h>
#include <ui-controls-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include "base/notreached.h"
#include "components/exo/wayland/server.h"

namespace exo::wayland {

namespace {

constexpr uint32_t kUiControlsVersion = 1;

void ui_controls_send_key_press(struct wl_client* client,
                                struct wl_resource* resource,
                                uint32_t key,
                                uint32_t pressed_modifiers,
                                uint32_t id) {
  NOTIMPLEMENTED();
}

void ui_controls_send_mouse_move(struct wl_client* client,
                                 struct wl_resource* resource,
                                 int32_t x,
                                 int32_t y,
                                 struct wl_resource* surface,
                                 uint32_t id) {
  NOTIMPLEMENTED();
}

void ui_controls_send_mouse_button(struct wl_client* client,
                                   struct wl_resource* resource,
                                   uint32_t button,
                                   uint32_t button_state,
                                   uint32_t pressed_modifiers,
                                   uint32_t id) {
  NOTIMPLEMENTED();
}

void ui_controls_send_touch(struct wl_client* client,
                            struct wl_resource* resource,
                            uint32_t action,
                            uint32_t touch_id,
                            int32_t x,
                            int32_t y,
                            struct wl_resource* surface,
                            uint32_t id) {
  NOTIMPLEMENTED();
}

void ui_controls_set_toplevel_bounds(struct wl_client* client,
                                     struct wl_resource* resource,
                                     struct wl_resource* toplevel,
                                     int32_t x,
                                     int32_t y,
                                     uint32_t width,
                                     uint32_t height) {
  NOTIMPLEMENTED();
}

const struct zcr_ui_controls_v1_interface ui_controls_implementation = {
    ui_controls_send_key_press, ui_controls_send_mouse_move,
    ui_controls_send_mouse_button, ui_controls_send_touch,
    ui_controls_set_toplevel_bounds};

void bind_ui_controls(wl_client* client,
                      void* data,
                      uint32_t version,
                      uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_ui_controls_v1_interface, version, id);

  wl_resource_set_implementation(resource, &ui_controls_implementation, data,
                                 nullptr);
}

}  // namespace

UiControls::UiControls(Server* server) {
  wl_global_create(server->GetWaylandDisplay(), &zcr_ui_controls_v1_interface,
                   kUiControlsVersion, nullptr, bind_ui_controls);
}

UiControls::~UiControls() = default;

}  // namespace exo::wayland

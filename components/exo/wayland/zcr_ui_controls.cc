// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_ui_controls.h"

#include <stdint.h>
#include <ui-controls-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include "ash/shell.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "ui/aura/env.h"
#include "ui/aura/env_input_state_controller.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace exo::wayland {

struct UiControls::UiControlsState {
  explicit UiControlsState(const Seat* seat) : seat_(seat) {}
  UiControlsState(const UiControlsState&) = delete;
  UiControlsState& operator=(const UiControlsState&) = delete;

  const Seat* const seat_;

  // Keeps track of the IDs of pending requests for that we still need to emit
  // request_processed events. This is per wl_resource so that we can drop
  // pending requests for a resource when the resource is destroyed.
  std::map<wl_resource*, std::set<uint32_t>> pending_request_ids_;
};

namespace {

using UiControlsState = UiControls::UiControlsState;

constexpr uint32_t kUiControlsVersion = 2;

base::OnceClosure UpdateStateAndBindEmitProcessed(struct wl_resource* resource,
                                                  uint32_t id) {
  auto* state = GetUserDataAs<UiControlsState>(resource);
  auto pending_request_ids =
      state->pending_request_ids_.try_emplace(resource).first->second;
  pending_request_ids.insert(id);

  return base::BindLambdaForTesting([=]() {
    // Ensure |resource| hasn't been destroyed in the meantime.
    if (base::Contains(state->pending_request_ids_, resource)) {
      zcr_ui_controls_v1_send_request_processed(resource, id);
      state->pending_request_ids_[resource].erase(id);
    }
  });
}

// Ensures that a crashed test doesn't leave behind pressed keys, mouse buttons,
// or touch points.
void ResetInputs(UiControlsState* state) {
  auto* window = ash::Shell::GetPrimaryRootWindow();
  auto pressed_keys = state->seat_->pressed_keys();
  for (auto key : pressed_keys) {
    auto key_code = ui::DomCodeToUsLayoutNonLocatedKeyboardCode(key.first);
    ui_controls::SendKeyEvents(window, key_code, ui_controls::kKeyRelease);
  }

  auto* env = aura::Env::GetInstance();
  int button_flags = env->mouse_button_flags();
  if (button_flags & ui::EF_LEFT_MOUSE_BUTTON) {
    ui_controls::SendMouseEvents(ui_controls::LEFT,
                                 ui_controls::MouseButtonState::UP);
  }
  if (button_flags & ui::EF_MIDDLE_MOUSE_BUTTON) {
    ui_controls::SendMouseEvents(ui_controls::MIDDLE,
                                 ui_controls::MouseButtonState::UP);
  }
  if (button_flags & ui::EF_RIGHT_MOUSE_BUTTON) {
    ui_controls::SendMouseEvents(ui_controls::RIGHT,
                                 ui_controls::MouseButtonState::UP);
  }

  uint32_t touch_ids_down = env->env_controller()->touch_ids_down();
  for (uint32_t touch_id = 0; touch_id < 32; ++touch_id) {
    if (touch_ids_down & (1 << touch_id)) {
      ui_controls::SendTouchEvents(ui_controls::TouchType::kTouchRelease,
                                   touch_id, 0, 0);
    }
  }
}

void ui_controls_send_key_events(struct wl_client* client,
                                 struct wl_resource* resource,
                                 uint32_t key,
                                 uint32_t key_state,
                                 uint32_t pressed_modifiers,
                                 uint32_t id) {
  auto emit_processed = UpdateStateAndBindEmitProcessed(resource, id);
  auto* window = ash::Shell::GetPrimaryRootWindow();
  auto dom_code = ui::KeycodeConverter::EvdevCodeToDomCode(key);
  auto key_code = ui::DomCodeToUsLayoutNonLocatedKeyboardCode(dom_code);
  ui_controls::SendKeyEventsNotifyWhenDone(window, key_code, key_state,
                                           std::move(emit_processed),
                                           pressed_modifiers);
}

void ui_controls_send_mouse_move(struct wl_client* client,
                                 struct wl_resource* resource,
                                 int32_t x,
                                 int32_t y,
                                 struct wl_resource* surface,
                                 uint32_t id) {
  if (surface) {
    LOG(WARNING)
        << "The `surface` parameter for ui_controls.send_mouse_move should be "
           "NULL on LaCrOS, but it isn't. Why aren't we using screen "
           "coordinates?";
  }

  auto emit_processed = UpdateStateAndBindEmitProcessed(resource, id);
  ui_controls::SendMouseMoveNotifyWhenDone(x, y, std::move(emit_processed));
}

void ui_controls_send_mouse_button(struct wl_client* client,
                                   struct wl_resource* resource,
                                   uint32_t button,
                                   uint32_t button_state,
                                   uint32_t pressed_modifiers,
                                   uint32_t id) {
  auto emit_processed = UpdateStateAndBindEmitProcessed(resource, id);
  ui_controls::SendMouseEventsNotifyWhenDone(
      static_cast<ui_controls::MouseButton>(button), button_state,
      std::move(emit_processed), pressed_modifiers);
}

void ui_controls_send_touch(struct wl_client* client,
                            struct wl_resource* resource,
                            uint32_t action,
                            uint32_t touch_id,
                            int32_t x,
                            int32_t y,
                            struct wl_resource* surface,
                            uint32_t id) {
  if (surface) {
    LOG(WARNING)
        << "The `surface` parameter for ui_controls.send_touch should be NULL "
           "on LaCrOS, but it isn't. Why aren't we using screen coordinates?";
  }

  auto emit_processed = UpdateStateAndBindEmitProcessed(resource, id);
  ui_controls::SendTouchEventsNotifyWhenDone(action, touch_id, x, y,
                                             std::move(emit_processed));
}

void ui_controls_set_toplevel_bounds(struct wl_client* client,
                                     struct wl_resource* resource,
                                     struct wl_resource* toplevel,
                                     int32_t x,
                                     int32_t y,
                                     uint32_t width,
                                     uint32_t height) {
  // Exo supports aura_shell, which already has an equivalent request. This
  // request only needs to be implemented by Weston.
  NOTIMPLEMENTED();
}

const struct zcr_ui_controls_v1_interface ui_controls_implementation = {
    ui_controls_send_key_events, ui_controls_send_mouse_move,
    ui_controls_send_mouse_button, ui_controls_send_touch,
    ui_controls_set_toplevel_bounds};

void destroy_ui_controls_resource(struct wl_resource* resource) {
  auto* state = GetUserDataAs<UiControlsState>(resource);
  state->pending_request_ids_.erase(resource);
  ResetInputs(state);
}

void bind_ui_controls(wl_client* client,
                      void* data,
                      uint32_t version,
                      uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_ui_controls_v1_interface, version, id);

  wl_resource_set_implementation(resource, &ui_controls_implementation, data,
                                 destroy_ui_controls_resource);
}

}  // namespace

UiControls::UiControls(Server* server)
    : state_(std::make_unique<UiControlsState>(server->GetDisplay()->seat())) {
  wl_global_create(server->GetWaylandDisplay(), &zcr_ui_controls_v1_interface,
                   kUiControlsVersion, state_.get(), bind_ui_controls);
}

UiControls::~UiControls() = default;

}  // namespace exo::wayland

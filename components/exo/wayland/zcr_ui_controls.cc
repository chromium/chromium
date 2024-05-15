// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_ui_controls.h"

#include <stdint.h>
#include <ui-controls-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include <limits>
#include <variant>

#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "base/bit_cast.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "ui/aura/env.h"
#include "ui/aura/env_input_state_controller.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/wayland/wayland_display_util.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace exo::wayland {

struct UiControls::UiControlsState {
  explicit UiControlsState(Server* server, const Seat* seat)
      : server_(server), seat_(seat) {}
  UiControlsState(const UiControlsState&) = delete;
  UiControlsState& operator=(const UiControlsState&) = delete;

  raw_ptr<Server> server_;
  const raw_ptr<const Seat> seat_;

  // Keeps track of the IDs of pending requests for that we still need to emit
  // request_processed events. This is per wl_resource so that we can drop
  // pending requests for a resource when the resource is destroyed.
  std::map<wl_resource*, std::set<uint32_t>> pending_request_ids_;

  // Keeps track of the original display spec to be restored on destroy.
  std::vector<display::ManagedDisplayInfo> original_displays_;
  // Pending display info to be added with display_info_done.
  std::optional<display::ManagedDisplayInfo> pending_display_;
  // Pending display info lists to be committed with display_info_list_done.
  std::vector<display::ManagedDisplayInfo> pending_display_info_list_;
};

namespace {

using UiControlsState = UiControls::UiControlsState;

constexpr uint32_t kUiControlsVersion =
    ZCR_UI_CONTROLS_V1_DISPLAY_INFO_LIST_DONE_SINCE_VERSION;

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

      // It can sometimes happen that the request_processed event gets stuck in
      // libwayland's queue without ever being sent, because the client is
      // waiting for the event and there is nothing else generating events. To
      // ensure the client actually receives the event, we need to flush
      // manually.
      state->server_->Flush();
    }
  });
}

// Ensures that a crashed test doesn't leave behind pressed keys, mouse buttons,
// or touch points.
void ResetInputs(UiControlsState* state) {
  auto* window = ash::Shell::GetPrimaryRootWindow();
  auto pressed_keys = state->seat_->pressed_keys();
  for (auto key : pressed_keys) {
    const ui::DomCode* physical_key = std::get_if<ui::DomCode>(&key.first);
    if (!physical_key) {
      continue;
    }
    auto key_code = ui::DomCodeToUsLayoutNonLocatedKeyboardCode(*physical_key);
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

  // TODO(crbug.com/40263572): Fix this issue and the code below should not be
  // necessary.
  ui_controls::SendMouseMove(0, 0);
}

// Ensure that the display is returned to the default setting.
void ResetDisplay(UiControlsState* state) {
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplayWithDisplayInfoList(state->original_displays_);
  ash::ScreenOrientationControllerTestApi(
      ash::Shell::Get()->screen_orientation_controller())
      .UpdateNaturalOrientation();
  state->pending_display_.reset();
  state->pending_display_info_list_.clear();
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

void ui_controls_set_display_info_id(struct wl_client* client,
                                     struct wl_resource* resource,
                                     uint32_t display_id_hi,
                                     uint32_t display_id_lo) {
  auto* state = GetUserDataAs<UiControlsState>(resource);

  int64_t display_id =
      ui::wayland::FromWaylandDisplayIdPair({display_id_hi, display_id_lo});
  if (!state->pending_display_) {
    state->pending_display_ =
        display::ManagedDisplayInfo::CreateFromSpecWithID({}, display_id);
  } else {
    state->pending_display_->set_display_id(display_id);
  }
}

void ui_controls_set_display_info_size(struct wl_client* client,
                                       struct wl_resource* resource,
                                       uint32_t width,
                                       uint32_t height) {
  auto* state = GetUserDataAs<UiControlsState>(resource);

  if (!state->pending_display_) {
    state->pending_display_ = display::ManagedDisplayInfo::CreateFromSpec(
        base::StringPrintf("%dx%d", width, height));
  } else {
    state->pending_display_->SetBounds(gfx::Rect(width, height));
  }
}

void ui_controls_set_display_info_device_scale_factor(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t scale_factor) {
  auto* state = GetUserDataAs<UiControlsState>(resource);
  static_assert(sizeof(uint32_t) == sizeof(float),
                "Sizes much match for reinterpret cast to be meaningful");
  // bit_cast is needed here because wayland doesn't support
  // float as primitive type and we are using 32 bits as storage.
  // static_cast won't work because the original value is integer.
  float device_scale_factor = base::bit_cast<float>(scale_factor);

  if (!state->pending_display_) {
    state->pending_display_ = display::ManagedDisplayInfo::CreateFromSpec({});
  }
  state->pending_display_->set_device_scale_factor(device_scale_factor);
}

void ui_controls_display_info_done(struct wl_client* client,
                                   struct wl_resource* resource) {
  auto* state = GetUserDataAs<UiControlsState>(resource);

  // Push info to pending display info list.
  state->pending_display_info_list_.push_back(*state->pending_display_);

  // Reset the state to default values.
  state->pending_display_.reset();
}

void ui_controls_display_info_list_done(struct wl_client* client,
                                        struct wl_resource* resource,
                                        uint32_t id) {
  auto emit_processed = UpdateStateAndBindEmitProcessed(resource, id);
  auto* state = GetUserDataAs<UiControlsState>(resource);

  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplayWithDisplayInfoList(state->pending_display_info_list_);
  ash::ScreenOrientationControllerTestApi(
      ash::Shell::Get()->screen_orientation_controller())
      .UpdateNaturalOrientation();
  state->pending_display_info_list_.clear();
  state->pending_display_.reset();

  std::move(emit_processed).Run();
}

const struct zcr_ui_controls_v1_interface ui_controls_implementation = {
    ui_controls_send_key_events,
    ui_controls_send_mouse_move,
    ui_controls_send_mouse_button,
    ui_controls_send_touch,
    ui_controls_set_display_info_id,
    ui_controls_set_display_info_size,
    ui_controls_set_display_info_device_scale_factor,
    ui_controls_display_info_done,
    ui_controls_display_info_list_done};

void destroy_ui_controls_resource(struct wl_resource* resource) {
  auto* state = GetUserDataAs<UiControlsState>(resource);
  state->pending_request_ids_.erase(resource);
  ResetInputs(state);
  ResetDisplay(state);
}

void bind_ui_controls(wl_client* client,
                      void* data,
                      uint32_t version,
                      uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_ui_controls_v1_interface,
                         std::min(version, kUiControlsVersion), id);

  wl_resource_set_implementation(resource, &ui_controls_implementation, data,
                                 destroy_ui_controls_resource);
}

}  // namespace

UiControls::UiControls(Server* server)
    : state_(std::make_unique<UiControlsState>(server,
                                               server->GetDisplay()->seat())) {
  wl_global_create(server->GetWaylandDisplay(), &zcr_ui_controls_v1_interface,
                   kUiControlsVersion, state_.get(), bind_ui_controls);

  auto* const display_manager = ash::Shell::Get()->display_manager();
  auto& display_list = display_manager->active_display_list();
  for (const display::Display& display : display_list) {
    state_->original_displays_.push_back(
        display_manager->GetDisplayInfo(display.id()));
  }
  // TODO(crbug.com/324562919) This hardcodes fling gesture detection to be
  // disabled when ui_controls is in use, so that it does not interfere with
  // tests that don't intend to trigger fling gestures. Some future tests will
  // intentionally trigger fling gestures, so this will need to become
  // configurable by clients at some point.
  ui::GestureConfiguration::GetInstance()->set_min_fling_velocity(
      std::numeric_limits<float>::max());
}

UiControls::~UiControls() = default;

}  // namespace exo::wayland

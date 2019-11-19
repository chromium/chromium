// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_gaming_input.h"

#include <gaming-input-unstable-v2-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/feature_list.h"
#include "base/macros.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gaming_seat.h"
#include "components/exo/gaming_seat_delegate.h"
#include "components/exo/wayland/server_util.h"
#include "ui/events/devices/gamepad_device.h"

namespace exo {
namespace wayland {

namespace {

unsigned int GetGamepadBusType(ui::InputDeviceType type) {
  switch (type) {
    case ui::INPUT_DEVICE_BLUETOOTH:
      return ZCR_GAMING_SEAT_V2_BUS_TYPE_BLUETOOTH;
    default:
      // Internal and unknown types also default to USB.
      return ZCR_GAMING_SEAT_V2_BUS_TYPE_USB;
  }
}

////////////////////////////////////////////////////////////////////////////////
// gaming_input_interface:

// Gamepad delegate class that forwards gamepad events to the client resource.
class WaylandGamepadDelegate : public GamepadDelegate {
 public:
  explicit WaylandGamepadDelegate(wl_resource* gamepad_resource)
      : gamepad_resource_(gamepad_resource) {}

  // If gamepad_resource_ is destroyed first, ResetGamepadResource will
  // be called to remove the resource from delegate, and delegate won't
  // do anything after that. If delegate is destructed first, it will
  // set the data to null in the gamepad_resource_, then the resource
  // destroy won't reset the delegate (cause it's gone).
  static void ResetGamepadResource(wl_resource* resource) {
    WaylandGamepadDelegate* delegate =
        GetUserDataAs<WaylandGamepadDelegate>(resource);
    if (delegate) {
      delegate->gamepad_resource_ = nullptr;
    }
  }

  // Override from GamepadDelegate:
  void OnRemoved() override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_removed(gamepad_resource_);
    wl_client_flush(client());
    // Reset the user data in gamepad_resource.
    wl_resource_set_user_data(gamepad_resource_, nullptr);
    delete this;
  }
  void OnAxis(int axis, double value) override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_axis(gamepad_resource_, NowInMilliseconds(), axis,
                             wl_fixed_from_double(value));
  }
  void OnButton(int button, bool pressed) override {
    if (!gamepad_resource_) {
      return;
    }
    uint32_t state = pressed ? ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED
                             : ZCR_GAMEPAD_V2_BUTTON_STATE_RELEASED;
    zcr_gamepad_v2_send_button(gamepad_resource_, NowInMilliseconds(), button,
                               state, wl_fixed_from_double(0));
  }
  void OnFrame() override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_frame(gamepad_resource_, NowInMilliseconds());
    wl_client_flush(client());
  }

 private:
  // The object should be deleted by OnRemoved().
  ~WaylandGamepadDelegate() override {}

  // The client who own this gamepad instance.
  wl_client* client() const {
    return wl_resource_get_client(gamepad_resource_);
  }

  // The gamepad resource associated with the gamepad.
  wl_resource* gamepad_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandGamepadDelegate);
};

void gamepad_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gamepad_v2_interface gamepad_implementation = {
    gamepad_destroy};

// GamingSeat delegate that provide gamepad added.
class WaylandGamingSeatDelegate : public GamingSeatDelegate {
 public:
  explicit WaylandGamingSeatDelegate(wl_resource* gaming_seat_resource)
      : gaming_seat_resource_{gaming_seat_resource} {}

  // Override from GamingSeatDelegate:
  void OnGamingSeatDestroying(GamingSeat*) override { delete this; }
  bool CanAcceptGamepadEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    return surface_resource &&
           wl_resource_get_client(surface_resource) ==
               wl_resource_get_client(gaming_seat_resource_);
  }
  GamepadDelegate* GamepadAdded(const ui::GamepadDevice& device) override {
    wl_resource* gamepad_resource =
        wl_resource_create(wl_resource_get_client(gaming_seat_resource_),
                           &zcr_gamepad_v2_interface,
                           wl_resource_get_version(gaming_seat_resource_), 0);

    GamepadDelegate* gamepad_delegate =
        new WaylandGamepadDelegate(gamepad_resource);

    wl_resource_set_implementation(
        gamepad_resource, &gamepad_implementation, gamepad_delegate,
        &WaylandGamepadDelegate::ResetGamepadResource);

    zcr_gaming_seat_v2_send_gamepad_added_with_device_info(
        gaming_seat_resource_, gamepad_resource, device.name.c_str(),
        GetGamepadBusType(device.type), device.vendor_id, device.product_id,
        device.version);

    for (const auto& axis : device.axes) {
      zcr_gamepad_v2_send_axis_added(gamepad_resource, axis.code,
                                     axis.min_value, axis.max_value, axis.flat,
                                     axis.fuzz, axis.resolution);
    }
    zcr_gamepad_v2_send_activated(gamepad_resource);
    wl_client_flush(wl_resource_get_client(gaming_seat_resource_));

    return gamepad_delegate;
  }

 private:
  // The gaming seat resource associated with the gaming seat.
  wl_resource* const gaming_seat_resource_;

  DISALLOW_COPY_AND_ASSIGN(WaylandGamingSeatDelegate);
};

void gaming_seat_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gaming_seat_v2_interface gaming_seat_implementation = {
    gaming_seat_destroy};

void gaming_input_get_gaming_seat(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* seat) {
  wl_resource* gaming_seat_resource =
      wl_resource_create(client, &zcr_gaming_seat_v2_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(gaming_seat_resource, &gaming_seat_implementation,
                    std::make_unique<GamingSeat>(
                        new WaylandGamingSeatDelegate(gaming_seat_resource)));
}

void gaming_input_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gaming_input_v2_interface gaming_input_implementation = {
    gaming_input_get_gaming_seat, gaming_input_destroy};

}  // namespace

void bind_gaming_input(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_gaming_input_v2_interface, version, id);

  wl_resource_set_implementation(resource, &gaming_input_implementation,
                                 nullptr, nullptr);
}

}  // namespace wayland
}  // namespace exo

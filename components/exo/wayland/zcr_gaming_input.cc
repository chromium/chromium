// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_gaming_input.h"

#include <gaming-input-unstable-v2-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/exo/gamepad.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gaming_seat.h"
#include "components/exo/gaming_seat_delegate.h"
#include "components/exo/wayland/server_util.h"
#include "ui/events/devices/gamepad_device.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

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

// Gamepad delegate class that forwards gamepad events to the client resource.
class WaylandGamepadDelegate : public GamepadDelegate {
 public:
  explicit WaylandGamepadDelegate(wl_resource* gamepad_resource)
      : gamepad_resource_(gamepad_resource) {}

  WaylandGamepadDelegate(const WaylandGamepadDelegate&) = delete;
  WaylandGamepadDelegate& operator=(const WaylandGamepadDelegate&) = delete;

  ~WaylandGamepadDelegate() override = default;

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
  }
  void OnAxis(int axis, double value, base::TimeTicks time_stamp) override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_axis(gamepad_resource_,
                             TimeTicksToMilliseconds(time_stamp), axis,
                             wl_fixed_from_double(value));
  }
  void OnButton(int button, bool pressed, base::TimeTicks time_stamp) override {
    if (!gamepad_resource_) {
      return;
    }
    uint32_t state = pressed ? ZCR_GAMEPAD_V2_BUTTON_STATE_PRESSED
                             : ZCR_GAMEPAD_V2_BUTTON_STATE_RELEASED;
    zcr_gamepad_v2_send_button(gamepad_resource_,
                               TimeTicksToMilliseconds(time_stamp), button,
                               state, wl_fixed_from_double(0));
  }
  void OnFrame(base::TimeTicks time_stamp) override {
    if (!gamepad_resource_) {
      return;
    }
    zcr_gamepad_v2_send_frame(gamepad_resource_,
                              TimeTicksToMilliseconds(time_stamp));
    wl_client_flush(client());
  }

  void ConfigureDevice(Gamepad* gamepad) {
    for (const auto& axis : gamepad->device.axes) {
      zcr_gamepad_v2_send_axis_added(gamepad_resource_, axis.code,
                                     axis.min_value, axis.max_value, axis.flat,
                                     axis.fuzz, axis.resolution);
    }

    if (wl_resource_get_version(gamepad_resource_) >=
        ZCR_GAMEPAD_V2_SUPPORTED_KEY_BITS_SINCE_VERSION) {
      // Sending key_bits.
      wl_array wl_key_bits;
      wl_array_init(&wl_key_bits);
      std::vector<uint64_t> key_bits =
          ui::OzonePlatform::GetInstance()
              ->GetInputController()
              ->GetGamepadKeyBits(gamepad->device.id);
      size_t key_bits_len = key_bits.size() * sizeof(uint64_t);
      uint64_t* wl_key_bits_ptr =
          static_cast<uint64_t*>(wl_array_add(&wl_key_bits, key_bits_len));
      if (wl_key_bits_ptr) {
        // SAFETY: wl_array_add allocated key_bits_len bytes, which is
        // key_bits.size() * sizeof(uint64_t) bytes. wl_key_bits_ptr is
        // uint64_t*, so the span size is key_bits.size().
        auto dest_span =
            UNSAFE_BUFFERS(base::span(wl_key_bits_ptr, key_bits.size()));
        dest_span.copy_from(key_bits);
        zcr_gamepad_v2_send_supported_key_bits(gamepad_resource_, &wl_key_bits);
      }
      wl_array_release(&wl_key_bits);
    }

    zcr_gamepad_v2_send_activated(gamepad_resource_);
  }

 private:
  // The client who own this gamepad instance.
  wl_client* client() const {
    return wl_resource_get_client(gamepad_resource_);
  }

  // The gamepad resource associated with the gamepad.
  raw_ptr<wl_resource> gamepad_resource_;
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

  WaylandGamingSeatDelegate(const WaylandGamingSeatDelegate&) = delete;
  WaylandGamingSeatDelegate& operator=(const WaylandGamingSeatDelegate&) =
      delete;

  // Override from GamingSeatDelegate:
  void OnGamingSeatDestroying(GamingSeat*) override { delete this; }
  bool CanAcceptGamepadEventsForSurface(Surface* surface) const override {
    wl_resource* surface_resource = GetSurfaceResource(surface);
    return surface_resource &&
           wl_resource_get_client(surface_resource) ==
               wl_resource_get_client(gaming_seat_resource_);
  }
  void GamepadAdded(Gamepad& gamepad) override {
    wl_resource* gamepad_resource =
        wl_resource_create(wl_resource_get_client(gaming_seat_resource_),
                           &zcr_gamepad_v2_interface,
                           wl_resource_get_version(gaming_seat_resource_), 0);

    zcr_gaming_seat_v2_send_gamepad_added_with_device_info(
        gaming_seat_resource_, gamepad_resource, gamepad.device.name.c_str(),
        GetGamepadBusType(gamepad.device.type), gamepad.device.vendor_id,
        gamepad.device.product_id, gamepad.device.version);

    std::unique_ptr<WaylandGamepadDelegate> gamepad_delegate =
        std::make_unique<WaylandGamepadDelegate>(gamepad_resource);

    wl_resource_set_implementation(
        gamepad_resource, &gamepad_implementation, gamepad_delegate.get(),
        &WaylandGamepadDelegate::ResetGamepadResource);

    gamepad_delegate->ConfigureDevice(&gamepad);
    gamepad.SetDelegate(std::move(gamepad_delegate));

    wl_client_flush(wl_resource_get_client(gaming_seat_resource_));
  }

  base::WeakPtr<GamingSeatDelegate> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // The gaming seat resource associated with the gaming seat.
  const raw_ptr<wl_resource> gaming_seat_resource_;

  base::WeakPtrFactory<WaylandGamingSeatDelegate> weak_factory_{this};
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

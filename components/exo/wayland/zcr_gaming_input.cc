// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/zcr_gaming_input.h"

#include <gaming-input-unstable-v2-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "components/exo/gamepad.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gamepad_observer.h"
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

// Handles the vibration requests sent by the client for a gamepad.
class WaylandGamepadVibratorImpl : public GamepadObserver {
 public:
  explicit WaylandGamepadVibratorImpl(Gamepad* gamepad) : gamepad_(gamepad) {
    gamepad_->AddObserver(this);
  }

  WaylandGamepadVibratorImpl(const WaylandGamepadVibratorImpl& other) = delete;
  WaylandGamepadVibratorImpl& operator=(
      const WaylandGamepadVibratorImpl& other) = delete;

  ~WaylandGamepadVibratorImpl() override {
    if (gamepad_)
      gamepad_->RemoveObserver(this);
  }

  void OnVibrate(wl_array* duration_millis,
                 wl_array* amplitudes,
                 int32_t repeat) {
    std::vector<int64_t> extracted_durations;
    int64_t* p;
    const uint8_t* duration_millis_end =
        static_cast<uint8_t*>(duration_millis->data) + duration_millis->size;
    for (p = static_cast<int64_t*>(duration_millis->data);
         (const uint8_t*)p < duration_millis_end; p++) {
      extracted_durations.emplace_back(*p);
    }

    const uint8_t* amplitudes_start = static_cast<uint8_t*>(amplitudes->data);
    size_t amplitude_size = amplitudes->size / sizeof(uint8_t);
    const uint8_t* amplitudes_end = amplitudes_start + amplitude_size;
    std::vector<uint8_t> extracted_amplitudes(amplitudes_start, amplitudes_end);

    if (gamepad_)
      gamepad_->Vibrate(extracted_durations, extracted_amplitudes, repeat);
  }

  void OnCancelVibration() {
    if (gamepad_)
      gamepad_->CancelVibration();
  }

  // Overridden from GamepadObserver
  void OnGamepadDestroying(Gamepad* gamepad) override {
    DCHECK_EQ(gamepad_, gamepad);
    gamepad_ = nullptr;
  }

 private:
  raw_ptr<Gamepad> gamepad_;
};

void gamepad_vibrator_vibrate(wl_client* client,
                              wl_resource* resource,
                              wl_array* duration_millis,
                              wl_array* amplitudes,
                              int32_t repeat) {
  GetUserDataAs<WaylandGamepadVibratorImpl>(resource)->OnVibrate(
      duration_millis, amplitudes, repeat);
}

void gamepad_vibrator_cancel_vibration(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<WaylandGamepadVibratorImpl>(resource)->OnCancelVibration();
}

void gamepad_vibrator_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_gamepad_vibrator_v2_interface gamepad_vibrator_implementation =
    {gamepad_vibrator_vibrate, gamepad_vibrator_cancel_vibration,
     gamepad_vibrator_destroy};

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

    if (gamepad->device.supports_vibration_rumble &&
        wl_resource_get_version(gamepad_resource_) >=
            ZCR_GAMEPAD_V2_VIBRATOR_ADDED_SINCE_VERSION) {
      wl_resource* gamepad_vibrator_resource =
          wl_resource_create(wl_resource_get_client(gamepad_resource_),
                             &zcr_gamepad_vibrator_v2_interface,
                             wl_resource_get_version(gamepad_resource_), 0);

      SetImplementation(gamepad_vibrator_resource,
                        &gamepad_vibrator_implementation,
                        std::make_unique<WaylandGamepadVibratorImpl>(gamepad));

      zcr_gamepad_v2_send_vibrator_added(gamepad_resource_,
                                         gamepad_vibrator_resource);
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
        memcpy(wl_key_bits_ptr, key_bits.data(), key_bits_len);
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

 private:
  // The gaming seat resource associated with the gaming seat.
  const raw_ptr<wl_resource> gaming_seat_resource_;
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

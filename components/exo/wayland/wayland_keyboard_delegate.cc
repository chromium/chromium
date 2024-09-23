// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/wayland/wayland_keyboard_delegate.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include <cstring>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "components/exo/wayland/serial_tracker.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include <xkbcommon/xkbcommon.h>
#endif

namespace exo {
namespace wayland {

#if BUILDFLAG(USE_XKBCOMMON)

WaylandKeyboardDelegate::WaylandKeyboardDelegate(wl_resource* keyboard_resource,
                                                 SerialTracker* serial_tracker)
    : keyboard_resource_(keyboard_resource), serial_tracker_(serial_tracker) {}

WaylandKeyboardDelegate::~WaylandKeyboardDelegate() = default;

bool WaylandKeyboardDelegate::CanAcceptKeyboardEventsForSurface(
    Surface* surface) const {
  wl_resource* surface_resource = GetSurfaceResource(surface);
  // We can accept events for this surface if the client is the same as the
  // keyboard.
  return surface_resource &&
         wl_resource_get_client(surface_resource) == client();
}

void WaylandKeyboardDelegate::OnKeyboardEnter(
    Surface* surface,
    const base::flat_map<PhysicalCode, base::flat_set<KeyState>>&
        pressed_keys) {
  wl_resource* surface_resource = GetSurfaceResource(surface);
  DCHECK(surface_resource);
  wl_array keys;
  wl_array_init(&keys);
  for (const auto& entry : pressed_keys) {
    for (const auto& key_state : entry.second) {
      uint32_t* value =
          static_cast<uint32_t*>(wl_array_add(&keys, sizeof(uint32_t)));
      DCHECK(value);
      *value = ui::KeycodeConverter::DomCodeToEvdevCode(key_state.code);
    }
  }
  wl_keyboard_send_enter(
      keyboard_resource_,
      serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
      surface_resource, &keys);
  wl_array_release(&keys);
  wl_client_flush(client());
}

void WaylandKeyboardDelegate::OnKeyboardLeave(Surface* surface) {
  wl_resource* surface_resource = GetSurfaceResource(surface);
  DCHECK(surface_resource);
  wl_keyboard_send_leave(
      keyboard_resource_,
      serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
      surface_resource);
  wl_client_flush(client());
}

uint32_t WaylandKeyboardDelegate::OnKeyboardKey(base::TimeTicks time_stamp,
                                                ui::DomCode code,
                                                bool pressed) {
  uint32_t serial = serial_tracker_->MaybeNextKeySerial();
  serial_tracker_->ResetKeySerial();
  SendTimestamp(time_stamp);
  wl_keyboard_send_key(
      keyboard_resource_, serial, TimeTicksToMilliseconds(time_stamp),
      ui::KeycodeConverter::DomCodeToEvdevCode(code),
      pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
  // Unlike normal wayland clients, the X11 server tries to maintain its own
  // modifier state, which it updates based on key events. To prevent numlock
  // presses from allowing numpad keys to be interpreted as directions, we
  // re-send the modifier state after a numlock press.
  if (code == ui::DomCode::NUM_LOCK)
    SendKeyboardModifiers();
  wl_client_flush(client());
  return serial;
}

void WaylandKeyboardDelegate::OnKeyboardModifiers(
    const KeyboardModifiers& modifiers) {
  // Send the update only when they're different.
  if (current_modifiers_ == modifiers)
    return;
  current_modifiers_ = modifiers;
  SendKeyboardModifiers();
}

void WaylandKeyboardDelegate::OnKeyboardLayoutUpdated(std::string_view keymap) {
  // Sent the content of |keymap| with trailing '\0' termination via shared
  // memory.
  base::UnsafeSharedMemoryRegion shared_keymap_region =
      base::UnsafeSharedMemoryRegion::Create(keymap.size() + 1);
  base::WritableSharedMemoryMapping shared_keymap = shared_keymap_region.Map();
  base::subtle::PlatformSharedMemoryRegion platform_shared_keymap =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shared_keymap_region));
  DCHECK(shared_keymap.IsValid());

  std::memcpy(shared_keymap.memory(), keymap.data(), keymap.size());
  static_cast<uint8_t*>(shared_keymap.memory())[keymap.size()] = '\0';
  wl_keyboard_send_keymap(keyboard_resource_, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                          platform_shared_keymap.GetPlatformHandle().fd,
                          keymap.size() + 1);
  wl_client_flush(client());
}

void WaylandKeyboardDelegate::SendKeyboardModifiers() {
  wl_keyboard_send_modifiers(
      keyboard_resource_,
      serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
      current_modifiers_.depressed, current_modifiers_.locked,
      current_modifiers_.latched, current_modifiers_.group);
  wl_client_flush(client());
}

// Convert from ChromeOS's key repeat interval to Wayland's key repeat rate.
// For example, an interval of 500ms is a rate of 1000/500 = 2 Hz.
//
// Known issue: A 2000ms interval is 0.5 Hz. This rounds to 1 Hz, which
// is twice as fast. This is not fixable without Wayland spec changes.
int32_t GetWaylandRepeatRate(bool enabled, base::TimeDelta interval) {
  DCHECK(interval.InMillisecondsF() > 0.0);
  int32_t rate;
  if (enabled) {
    // Most of ChromeOS's interval options divide perfectly into 1000,
    // but a few do need rounding.
    rate = base::ClampRound<int32_t>(interval.ToHz());

    // Avoid disabling key repeat if the interval is >2000ms.
    rate = std::max(1, rate);
  } else {
    // Disables key repeat, as documented in Wayland spec.
    rate = 0;
  }
  return rate;
}

// Expose GetWaylandRepeatRate() to tests.
int32_t GetWaylandRepeatRateForTesting(bool enabled, base::TimeDelta interval) {
  return GetWaylandRepeatRate(enabled, interval);
}

void WaylandKeyboardDelegate::OnKeyRepeatSettingsChanged(
    bool enabled,
    base::TimeDelta delay,
    base::TimeDelta interval) {
  // delay may be zero, but not negative (per Wayland spec).
  DCHECK_GE(delay.InMilliseconds(), 0);

  uint32_t version = wl_resource_get_version(keyboard_resource_);
  if (version >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION) {
    wl_keyboard_send_repeat_info(keyboard_resource_,
                                 GetWaylandRepeatRate(enabled, interval),
                                 static_cast<int32_t>(delay.InMilliseconds()));
    wl_client_flush(client());
  }
}

wl_client* WaylandKeyboardDelegate::client() const {
  return wl_resource_get_client(keyboard_resource_);
}

#endif  // BUILDFLAG(USE_XKBCOMMON)

}  // namespace wayland
}  // namespace exo

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_keyboard_delegate.h"

#include <cstring>

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/containers/flat_map.h"
#include "components/exo/wayland/serial_tracker.h"
#include "components/exo/xkb_tracker.h"
#include "ui/events/keycodes/dom/dom_code.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#endif

namespace exo {
namespace wayland {

#if BUILDFLAG(USE_XKBCOMMON)

WaylandKeyboardDelegate::WaylandKeyboardDelegate(wl_resource* keyboard_resource,
                                                 SerialTracker* serial_tracker)
    : keyboard_resource_(keyboard_resource),
      serial_tracker_(serial_tracker),
      xkb_tracker_(std::make_unique<XkbTracker>()) {
#if defined(OS_CHROMEOS)
  ash::ImeControllerImpl* ime_controller = ash::Shell::Get()->ime_controller();
  xkb_tracker_->UpdateKeyboardLayout(ime_controller->keyboard_layout_name());
  ime_controller->AddObserver(this);
#endif
  SendLayout();
}

WaylandKeyboardDelegate::~WaylandKeyboardDelegate() {
#if defined(OS_CHROMEOS)
  ash::Shell::Get()->ime_controller()->RemoveObserver(this);
#endif
}

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
    const base::flat_map<ui::DomCode, ui::DomCode>& pressed_keys) {
  wl_resource* surface_resource = GetSurfaceResource(surface);
  DCHECK(surface_resource);
  wl_array keys;
  wl_array_init(&keys);
  for (const auto& entry : pressed_keys) {
    uint32_t* value =
        static_cast<uint32_t*>(wl_array_add(&keys, sizeof(uint32_t)));
    DCHECK(value);
    *value = DomCodeToKey(entry.second);
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
                                                ui::DomCode key,
                                                bool pressed) {
  uint32_t serial =
      serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT);
  SendTimestamp(time_stamp);
  wl_keyboard_send_key(
      keyboard_resource_, serial, TimeTicksToMilliseconds(time_stamp),
      DomCodeToKey(key),
      pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
  // Unlike normal wayland clients, the X11 server tries to maintain its own
  // modifier state, which it updates based on key events. To prevent numlock
  // presses from allowing numpad keys to be interpreted as directions, we
  // re-send the modifier state after a numlock press.
  if (key == ui::DomCode::NUM_LOCK)
    SendKeyboardModifiers();
  wl_client_flush(client());
  return serial;
}

void WaylandKeyboardDelegate::OnKeyboardModifiers(int modifier_flags) {
  xkb_tracker_->UpdateKeyboardModifiers(modifier_flags);
  SendKeyboardModifiers();
}

#if defined(OS_CHROMEOS)
void WaylandKeyboardDelegate::OnCapsLockChanged(bool enabled) {}

void WaylandKeyboardDelegate::OnKeyboardLayoutNameChanged(
    const std::string& layout_name) {
  xkb_tracker_->UpdateKeyboardLayout(layout_name);
  SendLayout();
}
#endif

uint32_t WaylandKeyboardDelegate::DomCodeToKey(ui::DomCode code) const {
  // This assumes KeycodeConverter has been built with evdev/xkb codes.
  xkb_keycode_t xkb_keycode = static_cast<xkb_keycode_t>(
      ui::KeycodeConverter::DomCodeToNativeKeycode(code));

  // Keycodes are offset by 8 in Xkb.
  DCHECK_GE(xkb_keycode, 8u);
  return xkb_keycode - 8;
}

void WaylandKeyboardDelegate::SendKeyboardModifiers() {
  wl_keyboard_send_modifiers(
      keyboard_resource_,
      serial_tracker_->GetNextSerial(SerialTracker::EventType::OTHER_EVENT),
      xkb_tracker_->GetSerializeMods(XKB_STATE_MODS_DEPRESSED),
      xkb_tracker_->GetSerializeMods(XKB_STATE_MODS_LOCKED),
      xkb_tracker_->GetSerializeMods(XKB_STATE_MODS_LATCHED),
      xkb_tracker_->GetSerializeLayout(XKB_STATE_LAYOUT_EFFECTIVE));
  wl_client_flush(client());
}

void WaylandKeyboardDelegate::SendLayout() {
  auto keymap = xkb_tracker_->GetKeymap();
  size_t keymap_size = strlen(keymap.get()) + 1;

  base::UnsafeSharedMemoryRegion shared_keymap_region =
      base::UnsafeSharedMemoryRegion::Create(keymap_size);
  base::WritableSharedMemoryMapping shared_keymap = shared_keymap_region.Map();
  base::subtle::PlatformSharedMemoryRegion platform_shared_keymap =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shared_keymap_region));
  DCHECK(shared_keymap.IsValid());

  std::memcpy(shared_keymap.memory(), keymap.get(), keymap_size);
  wl_keyboard_send_keymap(keyboard_resource_, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                          platform_shared_keymap.GetPlatformHandle().fd,
                          keymap_size);
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
    rate = int32_t{std::lround(1000.0 / interval.InMillisecondsF())};

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
                                 int32_t{delay.InMilliseconds()});
  }
}

wl_client* WaylandKeyboardDelegate::client() const {
  return wl_resource_get_client(keyboard_resource_);
}

#endif  // BUILDFLAG(USE_XKBCOMMON)

}  // namespace wayland
}  // namespace exo

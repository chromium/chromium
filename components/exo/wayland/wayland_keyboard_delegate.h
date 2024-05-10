// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_KEYBOARD_DELEGATE_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_KEYBOARD_DELEGATE_H_

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_modifiers.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_input_delegate.h"
#include "ui/base/buildflags.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

struct wl_client;
struct wl_resource;

namespace exo {
namespace wayland {
class SerialTracker;

// Keyboard delegate class that accepts events for surfaces owned by the same
// client as a keyboard resource.
class WaylandKeyboardDelegate : public WaylandInputDelegate,
                                public KeyboardDelegate {
#if BUILDFLAG(USE_XKBCOMMON)
 public:
  WaylandKeyboardDelegate(wl_resource* keyboard_resource,
                          SerialTracker* serial_tracker);
  WaylandKeyboardDelegate(const WaylandKeyboardDelegate&) = delete;
  WaylandKeyboardDelegate& operator=(const WaylandKeyboardDelegate) = delete;
  ~WaylandKeyboardDelegate() override;

  // Overridden from KeyboardDelegate:
  bool CanAcceptKeyboardEventsForSurface(Surface* surface) const override;
  void OnKeyboardEnter(
      Surface* surface,
      const base::flat_map<PhysicalCode, base::flat_set<KeyState>>&
          pressed_keys) override;
  void OnKeyboardLeave(Surface* surface) override;
  uint32_t OnKeyboardKey(base::TimeTicks time_stamp,
                         ui::DomCode key,
                         bool pressed) override;
  void OnKeyboardModifiers(const KeyboardModifiers& modifiers) override;
  void OnKeyRepeatSettingsChanged(bool enabled,
                                  base::TimeDelta delay,
                                  base::TimeDelta interval) override;
  void OnKeyboardLayoutUpdated(std::string_view keymap) override;

 private:
  // Sends the current modifiers to the client.
  void SendKeyboardModifiers();

  // The client who own this keyboard instance.
  wl_client* client() const;

  // The keyboard resource associated with the keyboard.
  const raw_ptr<wl_resource> keyboard_resource_;

  // Owned by Server, which always outlives this delegate.
  const raw_ptr<SerialTracker> serial_tracker_;

  // Tracks the latest modifiers.
  KeyboardModifiers current_modifiers_{};
#endif
};

// Exposed for testing.
int32_t GetWaylandRepeatRateForTesting(bool enabled, base::TimeDelta interval);

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_KEYBOARD_DELEGATE_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_DELEGATE_H_
#define COMPONENTS_EXO_KEYBOARD_DELEGATE_H_

#include <string_view>

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/exo/key_state.h"

namespace ui {
enum class DomCode : uint32_t;
}

namespace exo {
struct KeyboardModifiers;
class Surface;

// Handles events on keyboards in context-specific ways.
class KeyboardDelegate {
 public:
  virtual ~KeyboardDelegate() = default;

  // This should return true if |surface| is a valid target for this keyboard.
  // E.g. the surface is owned by the same client as the keyboard.
  virtual bool CanAcceptKeyboardEventsForSurface(Surface* surface) const = 0;

  // Called when keyboard focus enters a new valid target surface.
  virtual void OnKeyboardEnter(
      Surface* surface,
      const base::flat_map<PhysicalCode, base::flat_set<KeyState>>&
          pressed_keys) = 0;

  // Called when keyboard focus leaves a valid target surface.
  virtual void OnKeyboardLeave(Surface* surface) = 0;

  // Called when keyboard key state changed. |pressed| is true when a key with
  // |code| was pressed and false if it was released. Should return the serial
  // number that will be used by the client to acknowledge the change in
  // key state.
  virtual uint32_t OnKeyboardKey(base::TimeTicks time_stamp,
                                 ui::DomCode code,
                                 bool pressed) = 0;

  // Called when keyboard modifier state changed.
  virtual void OnKeyboardModifiers(const KeyboardModifiers& modifiers) = 0;

  // Called when key repeat settings are changed.
  virtual void OnKeyRepeatSettingsChanged(bool enabled,
                                          base::TimeDelta delay,
                                          base::TimeDelta interval) = 0;

  // Called when keyboard layout is updated.
  virtual void OnKeyboardLayoutUpdated(std::string_view keymap) = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_DELEGATE_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/mojom/ime_mojom_traits.h"

#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace mojo {
using KeyEventUniquePtr = std::unique_ptr<ui::KeyEvent>;

bool StructTraits<arc::mojom::KeyEventDataDataView, KeyEventUniquePtr>::Read(
    arc::mojom::KeyEventDataDataView data,
    KeyEventUniquePtr* out) {
  const ui::EventType type =
      data.pressed() ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED;
  // TODO(yhanada): Currently we have no way to know the correct keyboard layout
  // here, so assuming US layout. Find a way to get the more precise DomCode.
  ui::DomCode dom_code = ui::UsLayoutKeyboardCodeToDomCode(
      static_cast<ui::KeyboardCode>(data.key_code()));
  if (dom_code == ui::DomCode::NONE) {
    // |data.key_code| doesn't give us a proper DomCode. Let's fall back to
    // scan_code.
    dom_code = ui::KeycodeConverter::EvdevCodeToDomCode(data.scan_code());
  }

  int flags = 0;
  if (data.is_shift_down())
    flags |= ui::EF_SHIFT_DOWN;
  if (data.is_control_down())
    flags |= ui::EF_CONTROL_DOWN;
  if (data.is_alt_down())
    flags |= ui::EF_ALT_DOWN;
  if (data.is_capslock_on())
    flags |= ui::EF_CAPS_LOCK_ON;

  ui::KeyboardCode key_code;
  ui::DomKey dom_key;
  if (!DomCodeToUsLayoutDomKey(dom_code, flags, &dom_key, &key_code))
    return false;

  *out = std::make_unique<ui::KeyEvent>(type, key_code, dom_code, flags,
                                        dom_key, base::TimeTicks::Now());
  return true;
}

}  // namespace mojo

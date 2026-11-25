// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/voice/hotkeys/hotkey_pref_util.h"

#include "ui/base/accelerators/command.h"

namespace voice {

ui::Accelerator ParseHotkeyPref(std::string_view pref_value) {
  if (pref_value.empty()) {
    return ui::Accelerator();
  }

  const ui::Accelerator accelerator =
      ui::Command::StringToAccelerator(pref_value);

  // StringToAccelerator() returns an empty accelerator when the pref can't be
  // parsed, and it already requires parsed hotkeys to have a modifier. This is
  // defense in depth: never register a modifier-less hotkey, which would
  // swallow ordinary keystrokes.
  if (ui::Accelerator::MaskOutKeyEventFlags(accelerator.modifiers()) == 0) {
    return ui::Accelerator();
  }

  return accelerator;
}

}  // namespace voice

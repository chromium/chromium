// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VOICE_HOTKEYS_HOTKEY_PREF_UTIL_H_
#define CHROME_BROWSER_VOICE_HOTKEYS_HOTKEY_PREF_UTIL_H_

#include <string_view>

#include "ui/base/accelerators/accelerator.h"

namespace voice {

// Parses the value of a hotkey pref, i.e. an accelerator string as accepted by
// ui::Command::StringToAccelerator(). Returns an empty accelerator if
// `pref_value` is empty, can't be parsed, or has no modifier.
ui::Accelerator ParseHotkeyPref(std::string_view pref_value);

}  // namespace voice

#endif  // CHROME_BROWSER_VOICE_HOTKEYS_HOTKEY_PREF_UTIL_H_

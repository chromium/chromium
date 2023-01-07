// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_WINDOW_PIN_TYPE_H_
#define CHROMEOS_UI_BASE_WINDOW_PIN_TYPE_H_

#include <ostream>

#include "base/component_export.h"

namespace chromeos {

// The window's pin type enum.
enum class WindowPinType {
  kNone,

  // The window is pinned on top of other windows.
  kPinned,

  // The window is pinned on top of other windows. It is similar to
  // kPinned but does not allow user to exit the mode by shortcut key.
  kTrustedPinned,
};

COMPONENT_EXPORT(CHROMEOS_UI_BASE)
std::ostream& operator<<(std::ostream& stream, WindowPinType pin_type);

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_WINDOW_PIN_TYPE_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEY_STATE_H_
#define COMPONENTS_EXO_KEY_STATE_H_

#include <tuple>
#include <variant>

#include "ui/events/keycodes/keyboard_codes.h"

namespace ash::mojom {
enum class CustomizableButton : int32_t;
}

namespace ui {
enum class DomCode : uint32_t;
}

namespace exo {

// Marks the type of physical code used to generate key events.
// ui::DomCode::NONE marks the absence of data.
using PhysicalCode = std::variant<ui::DomCode, ash::mojom::CustomizableButton>;

// Represents the current pressed key state.
struct KeyState {
  ui::DomCode code;
  bool consumed_by_ime;
  ui::KeyboardCode key_code;
};

inline bool operator==(const KeyState& lhs, const KeyState& rhs) {
  return lhs.code == rhs.code && lhs.consumed_by_ime == rhs.consumed_by_ime;
}

inline bool operator<(const KeyState& lhs, const KeyState& rhs) {
  return std::tie(lhs.code, lhs.consumed_by_ime) <
         std::tie(rhs.code, rhs.consumed_by_ime);
}

}  // namespace exo

#endif  // COMPONENTS_EXO_KEY_STATE_H_

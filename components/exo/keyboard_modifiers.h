// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_MODIFIERS_H_
#define COMPONENTS_EXO_KEYBOARD_MODIFIERS_H_

#include <stdint.h>

#include <tuple>

namespace exo {

// Represents keyboard modifiers.
struct KeyboardModifiers {
  uint32_t depressed;
  uint32_t locked;
  uint32_t latched;
  uint32_t group;
};

inline bool operator==(const KeyboardModifiers& lhs,
                       const KeyboardModifiers& rhs) {
  return std::tie(lhs.depressed, lhs.locked, lhs.latched, lhs.group) ==
         std::tie(rhs.depressed, rhs.locked, rhs.latched, rhs.group);
}

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_MODIFIERS_H_

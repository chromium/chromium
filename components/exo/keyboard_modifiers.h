// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_MODIFIERS_H_
#define COMPONENTS_EXO_KEYBOARD_MODIFIERS_H_

#include <stdint.h>

namespace exo {

// Represents keyboard modifiers.
struct KeyboardModifiers {
  uint32_t depressed;
  uint32_t locked;
  uint32_t latched;
  uint32_t group;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_MODIFIERS_H_

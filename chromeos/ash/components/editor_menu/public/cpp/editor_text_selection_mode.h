// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_TEXT_SELECTION_MODE_H_
#define CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_TEXT_SELECTION_MODE_H_

#include "base/component_export.h"

namespace chromeos::editor_menu {

enum class COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) EditorTextSelectionMode {
  // Feature operates with no text selection
  kNoSelection,
  // Feature operates with text selection
  kHasSelection,
};

}  // namespace chromeos::editor_menu

#endif  // CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_TEXT_SELECTION_MODE_H_

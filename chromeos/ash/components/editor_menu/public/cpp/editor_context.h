// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_CONTEXT_H_
#define CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_CONTEXT_H_

#include "base/component_export.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_text_selection_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/preset_text_query.h"

namespace chromeos::editor_menu {

struct COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) EditorContext {
  EditorContext(EditorMode mode,
                EditorTextSelectionMode selection_mode,
                bool consent_status_settled,
                PresetTextQueries preset_queries);
  EditorContext(const EditorContext&);
  ~EditorContext();

  bool operator==(const EditorContext&) const = default;

  EditorMode mode;
  EditorTextSelectionMode text_selection_mode;

  // Indicating whether the editor consent status is already determined or still
  // unset.
  bool consent_status_settled;
  PresetTextQueries preset_queries;
};

}  // namespace chromeos::editor_menu

#endif  // CHROMEOS_ASH_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_EDITOR_CONTEXT_H_

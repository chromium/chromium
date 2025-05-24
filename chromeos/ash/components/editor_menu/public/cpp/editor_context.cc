// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"

#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_text_selection_mode.h"

namespace chromeos::editor_menu {

EditorContext::EditorContext(EditorMode mode,
                             EditorTextSelectionMode selection_mode,
                             bool consent_status_settled,
                             PresetTextQueries queries)
    : mode(mode),
      text_selection_mode(selection_mode),
      consent_status_settled(consent_status_settled),
      preset_queries(queries) {}

EditorContext::EditorContext(const EditorContext&) = default;
EditorContext::~EditorContext() = default;

}  // namespace chromeos::editor_menu

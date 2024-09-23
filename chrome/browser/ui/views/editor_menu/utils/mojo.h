// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_MOJO_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_MOJO_H_

#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"

namespace chromeos::editor_menu {

// TODO(b/326847990): Move to mojom traits
PresetQueryCategory FromMojoPresetQueryCategory(
    const crosapi::mojom::EditorPanelPresetQueryCategory category);

// TODO(b/326847990): Move to mojom traits
crosapi::mojom::EditorPanelMode ToMojoEditorMode(EditorMode mode);

// TODO(b/326847990): Move to mojom traits
EditorMode FromMojoEditorMode(const crosapi::mojom::EditorPanelMode mode);

// TODO(b/326847990): Move to mojom traits
EditorContext FromMojoEditorContext(
    crosapi::mojom::EditorPanelContextPtr panel_context);

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_MOJO_H_

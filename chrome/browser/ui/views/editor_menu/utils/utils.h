// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_UTILS_H_

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace chromeos::editor_menu {

enum class CardType { kQuickAnswers = 0, kEditorMenu };

int GetEditorMenuWidth(int anchor_view_width);

// Helper to compute editor menu bounds that for the provided anchor view
// bounds. This tries to position the editor menu somewhere above/below/around
// the anchor view while keeping the editor menu on-screen. Provided anchor view
// bounds and returned editor menu bounds are both in screen coordinates.
gfx::Rect GetEditorMenuBounds(const gfx::Rect& anchor_view_bounds,
                              const gfx::Size& editor_menu_preferred_size);

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_UTILS_H_

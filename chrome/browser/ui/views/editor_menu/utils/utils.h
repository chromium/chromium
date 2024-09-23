// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_UTILS_H_

#include "ui/views/view.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos::editor_menu {

enum class CardType {
  // Currently `kDefault` can be either Quick Answers or Mahi Condensed Menu
  // Card.
  kDefault = 0,

  kEditorMenu = 1,

  kMahiDefaultMenu = 2,

  kMagicBoostOptInCard = 3,
};

// Spacing between the editor menu and the anchor view (context menu).
inline constexpr int kEditorMenuMarginDip = 8;

// Minimum width of the editor menu.
inline constexpr int kEditorMenuMinWidthDip = 320;
inline constexpr int kMahiMenuTopBottomMinWidthDip = 240;
inline constexpr int kBigEditorMenuMinWidthDip = 480;

// Helper to compute editor menu bounds that for the provided anchor view
// bounds. This tries to position the editor menu somewhere above/below/around
// the anchor view while keeping the editor menu on-screen. Provided anchor view
// bounds and returned editor menu bounds are both in screen coordinates.
//
// How does this work internally?
//
// Given the position of context menu (anchor_view_bounds), we need to find
// the best on-screen position to fit the editor menu widget.
//
// There 6 possible candidates of the position:
//
//  1. Top.
//  2. Bottom.
//  3. Top left corner.
//  4. Top right corner.
//  5. Bottom left corner.
//  6. Bottom right corner.
//
// Extract constraints:
//
//  1. Top and bottom (1&2) candidate have at least the same width as context
//     menu.
//  2. The width of all candidtes must be 320 px.
//  3. Side canddiates will move closer to cursor point vertically if
//     they are on the same side of cursor point.
//
// We will pick the best candidate based on the following priorities:
//
//  1) Maximize visible area.
//  2) Minimize the distance from editor menu widget to cursor point.
//
// +------------------------------------------------------------------------+
// |     +-------+                                                   screen |
// |     |   1   |                                                          |
// | +---+-------+---------+                                                |
// | | 3 |       |    4    |                                                |
// | +---+       +---------+                                                |
// |     |  menu |                                                          |
// | +---+       +---------+                                                |
// | | 5 |       |    6    |                                                |
// | +---+-------+---------+                                                |
// |     |   2   |                                                          |
// |     +-------+                                                          |
// |                                                                        |
// |                                                                        |
// +------------------------------------------------------------------------+
//
gfx::Rect GetEditorMenuBounds(const gfx::Rect& anchor_view_bounds,
                              const views::View* target,
                              const CardType card_type = CardType::kDefault);

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_UTILS_H_

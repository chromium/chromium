// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_LAYOUT_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/focus_ring.h"

namespace projects_panel {

// The corner radius of a list item.
inline constexpr int kListItemCornerRadius = 8;

// The focus ring halo inset for a list item. This contains the focus ring
// within the bounds of the item so the halo isn't clipped by the container.
inline constexpr float kListItemFocusRingHaloInset =
    views::FocusRing::kDefaultHaloThickness / 2;

// The size of icons within a list item, in pixels.
inline constexpr int kListItemIconSize = 16;

// The padding around a list item.
inline constexpr gfx::Insets kListItemPadding = gfx::Insets::VH(8, 10);

// The preferred size of a list item.
inline constexpr gfx::Size kListItemPreferredSize = gfx::Size(0, 32);

// The padding around the title within a list item.
inline constexpr gfx::Insets kListItemTitlePadding =
    gfx::Insets::TLBR(0, 16, 0, 0);

// The padding around a list.
inline constexpr gfx::Insets kListPadding = gfx::Insets::VH(0, 4);

// Minimum width of the projects panel.
inline constexpr int kProjectsPanelMinWidth = 240;

// Background color of the projects panel.
inline constexpr ui::ColorId kProjectsPanelBackgroundColor =
    ui::kColorSysSurface2;

}  // namespace projects_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_LAYOUT_CONSTANTS_H_

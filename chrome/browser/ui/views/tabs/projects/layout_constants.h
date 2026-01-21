// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_LAYOUT_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"

namespace projects_panel {

// The size of icons within a list item, in pixels.
inline constexpr int kListItemIconSize = 16;

// The padding around a list item.
inline constexpr gfx::Insets kListItemPadding = gfx::Insets::VH(6, 10);

// The preferred size of a list item.
inline constexpr gfx::Size kListItemPreferredSize = gfx::Size(232, 24);

// The padding around the title within a list item.
inline constexpr gfx::Insets kListItemTitlePadding = gfx::Insets::VH(0, 16);

// The padding around a list.
inline constexpr gfx::Insets kListPadding = gfx::Insets::VH(0, 4);

}  // namespace projects_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_LAYOUT_CONSTANTS_H_

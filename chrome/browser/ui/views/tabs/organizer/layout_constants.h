// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_LAYOUT_CONSTANTS_H_

#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"

namespace projects_panel {

// Minimum width of the projects panel.
inline constexpr int kProjectsPanelMinWidth = 240;

// Background color of the projects panel.
inline constexpr ui::ColorId kProjectsPanelBackgroundColor =
    ui::kColorSysSurface2;

// Interior margins for the panel.
inline constexpr gfx::Insets kProjectsPanelRegionInteriorMargins =
    gfx::Insets::VH(12, 12);

}  // namespace projects_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_LAYOUT_CONSTANTS_H_

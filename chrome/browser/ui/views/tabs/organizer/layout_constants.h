// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_LAYOUT_CONSTANTS_H_

#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"

namespace organizer_panel {

// Minimum width of the organizer panel.
inline constexpr int kOrganizerPanelMinWidth = 240;

// Background color of the organizer panel.
inline constexpr ui::ColorId kOrganizerPanelBackgroundColor =
    ui::kColorSysSurface2;

// Margins for the panel controls view.
inline constexpr gfx::Insets kOrganizerPanelControlsMargins =
    gfx::Insets::VH(8, 8);

}  // namespace organizer_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_LAYOUT_CONSTANTS_H_

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

// Duration of the hover fade in/out animation.
inline constexpr auto kListItemHoverFadeAnimationDuration =
    base::Milliseconds(200);

// The margins around a list item.
inline constexpr gfx::Insets kListItemMargins = gfx::Insets(4);

// The spacing between children within a list item. Since FlexLayout does not
// provide an easy way to apply this, the spacing is added to the children's
// margins.
inline constexpr int kListItemSpacingBetweenChildren = 10;

// The margins around the title within a list item.
inline constexpr gfx::Insets kListItemTitleMargins =
    gfx::Insets::TLBR(2,
                      2 + projects_panel::kListItemSpacingBetweenChildren,
                      2,
                      2);

// The padding around a list.
inline constexpr gfx::Insets kListPadding = gfx::Insets::VH(0, 4);

// The size of the Tab groups icon.
inline constexpr int kTabGroupIconSize = 12;

// The margins for the Tab groups icon.
inline constexpr auto kTabGroupIconMargins = gfx::Insets(6);

// Margins for the lists separator.
inline constexpr gfx::Insets kListsSeparatorMargins = gfx::Insets::VH(12, 8);

// Maximum number of recent threads displayed in the UI.
// This value affects how the `Projects.ProjectsPanel.Threads.CountOnPanelOpen`
// histogram is recorded. If it needs to be changed, audit its uses and rename
// any affected histograms.
inline constexpr size_t kMaxNumberOfRecentThreads = 300;

// Number of threads visible when the threads section when collapsed.
inline constexpr int kNumThreadsVisibleWhenCollapsed = 3;

// Minimum width of the projects panel.
inline constexpr int kProjectsPanelMinWidth = 240;

// Background color of the projects panel.
inline constexpr ui::ColorId kProjectsPanelBackgroundColor =
    ui::kColorSysSurface2;

// Interior margins for the panel.
inline constexpr gfx::Insets kProjectsPanelRegionInteriorMargins =
    gfx::Insets::VH(12, 12);

// Insets for an item's trailing icon.
inline constexpr gfx::Insets kTrailingIconMargins =
    gfx::Insets::TLBR(3,
                      3 + projects_panel::kListItemSpacingBetweenChildren,
                      3,
                      3);

// Height and width of an item's trailing icon.
inline constexpr int kTrailingIconSize = 18;

}  // namespace projects_panel

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_PROJECTS_LAYOUT_CONSTANTS_H_

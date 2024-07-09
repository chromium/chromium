// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/utils.h"

#include "chrome/browser/browser_process.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos::editor_menu {

namespace {

std::string GetSystemLocale() {
  return g_browser_process != nullptr
             ? g_browser_process->GetApplicationLocale()
             : "";
}

int ComputeWidthOnSide() {
  if (GetSystemLocale() == "ta") {
    return kBigEditorMenuMinWidthDip;
  }
  return kEditorMenuMinWidthDip;
}

std::vector<gfx::Rect> GetEditorMenuBoundsCandidates(
    const gfx::Rect& anchor_view_bounds,
    const views::View* target,
    const gfx::Rect screen_work_area,
    const gfx::Point cursor_point,
    const CardType& card_type) {
  const int width_on_top_or_bottom = std::max(
      card_type == CardType::kMahiDefaultMenu ? kMahiMenuTopBottomMinWidthDip
                                              : kEditorMenuMinWidthDip,
      anchor_view_bounds.width());
  const int height_on_top_or_bottom =
      target->GetHeightForWidth(width_on_top_or_bottom);

  const int width_on_side = ComputeWidthOnSide();
  const int height_on_side = target->GetHeightForWidth(width_on_side);

  // The vertical starting position of top side candidates which makes them be
  // included in context menu range but also closer to cursor point.
  const int side_top = std::min(
      std::max(anchor_view_bounds.y(),
               cursor_point.y() - height_on_side - kEditorMenuMarginDip),
      anchor_view_bounds.y());

  // The vertical starting position of bottom side candidates which makes them
  // be included in context menu range but also closer to cursor point.
  const int side_bottom =
      std::max(std::min(anchor_view_bounds.bottom() - height_on_side,
                        cursor_point.y() + kEditorMenuMarginDip),
               anchor_view_bounds.y());

  const bool is_cursor_on_left =
      cursor_point.x() <
      (anchor_view_bounds.x() + anchor_view_bounds.width() / 2);
  const bool is_cursor_on_right =
      cursor_point.x() >
      (anchor_view_bounds.x() + anchor_view_bounds.width() / 2);

  const int side_top_left =
      is_cursor_on_left ? side_top : anchor_view_bounds.y();
  const int side_top_right =
      is_cursor_on_right ? side_top : anchor_view_bounds.y();

  const int side_bottom_left =
      is_cursor_on_left ? side_bottom
                        : anchor_view_bounds.bottom() - height_on_side;
  const int side_bottom_right =
      is_cursor_on_right ? side_bottom
                         : anchor_view_bounds.bottom() - height_on_side;

  std::vector<gfx::Rect> candidates = {

      // 1.a top (align with left edge).
      {
          gfx::Point(
              /*x=*/anchor_view_bounds.x(),
              /*y=*/anchor_view_bounds.y() - kEditorMenuMarginDip -
                  height_on_top_or_bottom),
          gfx::Size(
              /*width=*/width_on_top_or_bottom,
              /*height=*/height_on_top_or_bottom),
      },

      // 1.b top (align with right edge).
      {
          gfx::Point(
              /*x=*/anchor_view_bounds.x() + anchor_view_bounds.width() -
                  width_on_top_or_bottom,
              /*y=*/anchor_view_bounds.y() - kEditorMenuMarginDip -
                  height_on_top_or_bottom),
          gfx::Size(
              /*width=*/width_on_top_or_bottom,
              /*height=*/height_on_top_or_bottom),
      },

      // 2.a bottom (align with left edge).
      {
          gfx::Point(
              /*x=*/anchor_view_bounds.x(),
              /*y=*/anchor_view_bounds.bottom() + kEditorMenuMarginDip),
          gfx::Size(
              /*width=*/width_on_top_or_bottom,
              /*height=*/height_on_top_or_bottom),
      },

      // 2.b bottom (align with right edge).
      {
          gfx::Point(
              /*x=*/anchor_view_bounds.x() + anchor_view_bounds.width() -
                  width_on_top_or_bottom,
              /*y=*/anchor_view_bounds.bottom() + kEditorMenuMarginDip),
          gfx::Size(
              /*width=*/width_on_top_or_bottom,
              /*height=*/height_on_top_or_bottom),
      },

      // 3. top left
      {
          gfx::Point(
              /*x=*/anchor_view_bounds.x() - kEditorMenuMarginDip -
                  width_on_side,
              /*y=*/side_top_left),
          gfx::Size(
              /*width=*/width_on_side,
              /*height=*/height_on_side),
      },

      // 4. top right
      {
          gfx::Point(
              /*x=*/anchor_view_bounds.right() + kEditorMenuMarginDip,
              /*y=*/side_top_right),
          gfx::Size(
              /*width=*/width_on_side,
              /*height=*/height_on_side),
      },

      // 5. bottom left
      {
          gfx::Point(
              /*x=*/anchor_view_bounds.x() - kEditorMenuMarginDip -
                  width_on_side,
              /*y=*/side_bottom_left),
          gfx::Size(
              /*width=*/width_on_side,
              /*height=*/height_on_side),
      },

      // 6. bottom right
      {
          gfx::Point(
              /*x=*/anchor_view_bounds.right() + kEditorMenuMarginDip,
              /*y=*/side_bottom_right),
          gfx::Size(
              /*width=*/width_on_side,
              /*height=*/height_on_side),
      },
  };

  return candidates;
}

// Pick the best editor menu bounds from candidates.
//
// The best candidate is the one with the largest visible area.
// If there are multiple candidates with the same visible area, pick the one
// closest to the cursor.
gfx::Rect PickBestEditorMenuBounds(std::vector<gfx::Rect> candidates,
                                   const gfx::Rect screen_work_area,
                                   const gfx::Point cursor_point) {
  gfx::Rect best_candidate({0, 0}, {0, 0});
  int best_visible_score = 0;
  double best_cursor_distance = 1E9;

  for (const auto& candidate : candidates) {
    // The area of intersection between candidate and screen work area.
    const int visible_area =
        (std::min(candidate.right(), screen_work_area.right()) -
         std::max(candidate.x(), screen_work_area.x())) *
        (std::min(candidate.bottom(), screen_work_area.bottom()) -
         std::max(candidate.y(), screen_work_area.y()));

    const int total_area = candidate.width() * candidate.height();

    const int visible_score =
        total_area == 0 ? 0 : 100 * visible_area / total_area;

    const double cursor_distance =
        sqrt(pow(candidate.x() + candidate.width() / 2 - cursor_point.x(), 2) +
             pow(candidate.y() + candidate.height() / 2 - cursor_point.y(), 2));

    if (visible_score > best_visible_score ||
        (visible_score == best_visible_score &&
         cursor_distance < best_cursor_distance)) {
      best_candidate = candidate;
      best_visible_score = visible_score;
      best_cursor_distance = cursor_distance;
    }
  }

  return best_candidate;
}

}  // namespace

gfx::Rect GetEditorMenuBounds(const gfx::Rect& anchor_view_bounds,
                              const views::View* target,
                              const CardType card_type) {
  display::Screen* screen = display::Screen::GetScreen();
  const gfx::Rect screen_work_area =
      screen->GetDisplayMatching(anchor_view_bounds).work_area();
  const gfx::Point cursor_point = screen->GetCursorScreenPoint();

  std::vector<gfx::Rect> candidates = GetEditorMenuBoundsCandidates(
      anchor_view_bounds, target, screen_work_area, cursor_point, card_type);
  return PickBestEditorMenuBounds(candidates, screen_work_area, cursor_point);
}

}  // namespace chromeos::editor_menu

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lobster/lobster_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

constexpr int kLobsterInitialWidth = 440;
constexpr int kLobsterInitialHeight = 400;

constexpr int kLobsterAnchorVerticalPadding = 16;
constexpr int kLobsterScreenEdgePadding = 16;
constexpr int kLobsterResultCornerRadius = 20;

gfx::Rect ComputeInitialWidgetBounds(gfx::Rect caret_bounds,
                                     gfx::Insets inset,
                                     bool can_fallback_to_center_position) {
  gfx::Rect screen_work_area = display::Screen::GetScreen()
                                   ->GetDisplayMatching(caret_bounds)
                                   .work_area();
  screen_work_area.Inset(kLobsterScreenEdgePadding);

  gfx::Size initial_size =
      gfx::Size(kLobsterInitialWidth, kLobsterInitialHeight);

  // Otherwise, try to place it under at the bottom left of the selection.
  gfx::Rect anchor = caret_bounds;
  anchor.Outset(gfx::Outsets::VH(kLobsterAnchorVerticalPadding, 0));

  gfx::Rect lobster_contents_bounds =
      can_fallback_to_center_position &&
              (caret_bounds == gfx::Rect() ||
               !screen_work_area.Contains(caret_bounds))
          ? gfx::Rect(screen_work_area.x() + screen_work_area.width() / 2 -
                          initial_size.width() / 2,
                      screen_work_area.y() + screen_work_area.height() / 2 -
                          initial_size.height() / 2,
                      initial_size.width(), initial_size.height())
          : gfx::Rect(anchor.bottom_left(), initial_size);

  // If horizontally offscreen, just move it to the right edge of the screen.
  if (lobster_contents_bounds.right() > screen_work_area.right()) {
    lobster_contents_bounds.set_x(screen_work_area.right() -
                                  initial_size.width());
  }

  // If vertically offscreen, try above the selection.
  if (lobster_contents_bounds.bottom() > screen_work_area.bottom()) {
    lobster_contents_bounds.set_y(anchor.y() - initial_size.height());
  }

  // If still vertically offscreen, just move it to the bottom of the screen.
  if (lobster_contents_bounds.y() < screen_work_area.y()) {
    lobster_contents_bounds.set_y(screen_work_area.bottom() -
                                  initial_size.height());
  }

  // Compute widget bounds, which includes the border and shadow around the main
  // contents. Then, adjust again to ensure the whole widget is onscreen.
  gfx::Rect widget_bounds(lobster_contents_bounds);
  widget_bounds.Inset(inset);
  widget_bounds.AdjustToFit(screen_work_area);

  return widget_bounds;
}

}  // namespace

LobsterView::LobsterView(WebUIContentsWrapper* contents_wrapper,
                         const gfx::Rect& caret_bounds)
    : WebUIBubbleDialogView(nullptr,
                            contents_wrapper->GetWeakPtr(),
                            std::nullopt,
                            views::BubbleBorder::TOP_RIGHT,
                            /*autosize=*/false),
      caret_bounds_(caret_bounds) {
  set_has_parent(false);
  set_corner_radius(kLobsterResultCornerRadius);
  // Disable the default offscreen adjustment so that we can customise it.
  set_adjust_if_offscreen(false);
}

LobsterView::~LobsterView() = default;

void LobsterView::ShowUI() {
  WebUIBubbleDialogView::ShowUI();
  if (initial_bounds_set) {
    return;
  }
  GetWidget()->SetBounds(ComputeInitialWidgetBounds(
      caret_bounds_, -GetBubbleFrameView()->bubble_border()->GetInsets(),
      true));
  initial_bounds_set = true;
}

void LobsterView::SetContentsBounds(content::WebContents* source,
                                    const gfx::Rect& bounds) {
  if (views::WebView* web_view_ptr = web_view()) {
    web_view_ptr->SetPreferredSize(bounds.size());
  }
  GetWidget()->SetBounds(bounds);
}

BEGIN_METADATA(LobsterView)
END_METADATA

}  // namespace ash

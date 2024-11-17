// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lobster/lobster_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

constexpr int kLobsterAnchorVerticalPadding = 16;
constexpr int kLobsterScreenEdgePadding = 16;
constexpr int kLobsterResultCornerRadius = 20;

constexpr int kLobsterHeightThreshold = 400;

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

void LobsterView::ResizeDueToAutoResize(content::WebContents* source,
                                        const gfx::Size& new_size) {
  WebUIBubbleDialogView::ResizeDueToAutoResize(source, new_size);

  gfx::Rect screen_work_area = display::Screen::GetScreen()
                                   ->GetDisplayMatching(caret_bounds_)
                                   .work_area();
  screen_work_area.Inset(kLobsterScreenEdgePadding);

  // If the contents is very tall, just place it at the center of the screen.
  if (new_size.height() > kLobsterHeightThreshold) {
    SetArrowWithoutResizing(views::BubbleBorder::FLOAT);
    SetAnchorRect(screen_work_area);
    return;
  }

  // Otherwise, try to place it under at the bottom left of the selection.
  gfx::Rect anchor = caret_bounds_;
  anchor.Outset(gfx::Outsets::VH(kLobsterAnchorVerticalPadding, 0));

  gfx::Rect lobster_contents_bounds =
      gfx::Rect(screen_work_area.x() + screen_work_area.width() / 2 -
                    new_size.width() / 2,
                screen_work_area.y() + screen_work_area.height() / 2 -
                    new_size.height() / 2,
                new_size.width(), new_size.height());

  // If horizontally offscreen, just move it to the right edge of the screen.
  if (lobster_contents_bounds.right() > screen_work_area.right()) {
    lobster_contents_bounds.set_x(screen_work_area.right() - new_size.width());
  }

  // If vertically offscreen, try above the selection.
  if (lobster_contents_bounds.bottom() > screen_work_area.bottom()) {
    lobster_contents_bounds.set_y(anchor.y() - new_size.height());
  }
  // If still vertically offscreen, just move it to the bottom of the screen.
  if (lobster_contents_bounds.y() < screen_work_area.y()) {
    lobster_contents_bounds.set_y(screen_work_area.bottom() -
                                  new_size.height());
  }

  // Compute widget bounds, which includes the border and shadow around the main
  // contents. Then, adjust again to ensure the whole widget is onscreen.
  gfx::Rect widget_bounds(lobster_contents_bounds);
  widget_bounds.Inset(-GetBubbleFrameView()->bubble_border()->GetInsets());
  widget_bounds.AdjustToFit(screen_work_area);

  GetWidget()->SetBounds(widget_bounds);
}

void LobsterView::ShowUI() {
  WebUIBubbleDialogView::ShowUI();
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

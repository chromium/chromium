// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/compose_dialog_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/bubble/bubble_border.h"

namespace {

constexpr int kComposeDialogWorkAreaPadding = 16;
constexpr int kComposeDialogAnchorPadding = 0;

constexpr int kComposeMaxDialogHeightPx = 366;
constexpr int kComposeMaxDialogWidthPx = 448;

}  // namespace

ComposeDialogView::~ComposeDialogView() = default;

ComposeDialogView::ComposeDialogView(
    View* anchor_view,
    std::unique_ptr<BubbleContentsWrapperT<ComposeUI>> bubble_wrapper,
    const gfx::Rect& anchor_bounds,
    views::BubbleBorder::Arrow anchor_position)
    : WebUIBubbleDialogView(anchor_view,
                            bubble_wrapper->GetWeakPtr(),
                            anchor_bounds,
                            anchor_position),
      anchor_bounds_(anchor_bounds),
      bubble_wrapper_(std::move(bubble_wrapper)) {
  set_has_parent(false);
}

void ComposeDialogView::ResizeDueToAutoResize(content::WebContents* source,
                                              const gfx::Size& new_size) {
  WebUIBubbleDialogView::ResizeDueToAutoResize(source, new_size);
  gfx::Rect screen_work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(GetWidget()->GetNativeWindow())
          .work_area();

  // We don't want to render anything within `padding` pixels of the edge of the
  // screen work area.
  screen_work_area.Inset(kComposeDialogWorkAreaPadding);
  gfx::Rect anchor = anchor_bounds_;

  // We don't want to render anything within `padding` pixels of the edge of the
  // anchor rect.  But we will if we have to (due to AdjustToFit below).
  anchor.Outset(kComposeDialogAnchorPadding);

  // Available space measures the distance from each side of the padded work
  // area to the edge of the padded anchor (plus padding).
  gfx::Insets available_space = screen_work_area.InsetsFrom(anchor);

  // Ideally we render at the bottom left of the anchor.  If the dialog would be
  // offscreen, we reposition it.
  const gfx::Size widget_size = GetWidget()->GetWindowBoundsInScreen().size();
  gfx::Rect best_location(anchor.bottom_left(), widget_size);
  if (available_space.bottom() < kComposeMaxDialogHeightPx) {
    // Not enough space in preferred location. Try other locations
    if (available_space.top() >= kComposeMaxDialogHeightPx) {
      best_location.set_y(anchor.y() - best_location.height());
    } else if (available_space.right() > kComposeMaxDialogWidthPx) {
      best_location.set_origin(anchor.top_right());
    } else if (available_space.left() > kComposeMaxDialogWidthPx) {
      best_location.set_x(anchor.x() - best_location.width());
      best_location.set_y(anchor.y());
    } else {
      // fallback, nowhere works cleanly. Try to place it near the top right.
      best_location.set_origin(anchor.top_right());
    }
  }

  best_location.AdjustToFit(screen_work_area);
  GetWidget()->SetBounds(best_location);
}

base::WeakPtr<ComposeDialogView> ComposeDialogView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(ComposeDialogView, views::View)
END_METADATA

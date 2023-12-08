// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_rewrite_view.h"

#include "content/public/common/input/native_web_keyboard_event.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

constexpr int kMakoRewritePadding = 16;
constexpr int kMakoRewriteCornerRadius = 20;

// Height threshold of the mako rewrite UI which determines its screen position.
// Tall UI is centered on the display screen containing the caret, while short
// UI is anchored at the caret.
constexpr int kMakoRewriteHeightThreshold = 400;

// Default caret height to use when the actual caret height is unknown.
constexpr int kDefaultCaretHeight = 10;

}  // namespace

MakoRewriteView::MakoRewriteView(BubbleContentsWrapper* contents_wrapper,
                                 const gfx::Rect& caret_bounds)
    : WebUIBubbleDialogView(nullptr, contents_wrapper->GetWeakPtr()),
      caret_bounds_(caret_bounds) {
  set_has_parent(false);
  set_corner_radius(kMakoRewriteCornerRadius);
  // Disable the default offscreen adjustment so that we can customise it.
  set_adjust_if_offscreen(false);
}

MakoRewriteView::~MakoRewriteView() = default;

void MakoRewriteView::ResizeDueToAutoResize(content::WebContents* source,
                                            const gfx::Size& new_size) {
  WebUIBubbleDialogView::ResizeDueToAutoResize(source, new_size);
  const gfx::Rect screen_work_area = display::Screen::GetScreen()
                                         ->GetDisplayMatching(caret_bounds_)
                                         .work_area();

  // If the UI is very tall, just place it at the center of the screen.
  if (new_size.height() > kMakoRewriteHeightThreshold) {
    SetArrowWithoutResizing(views::BubbleBorder::FLOAT);
    SetAnchorRect(screen_work_area);
    return;
  }

  // Otherwise, try to place it near the selection. First, try to left align
  // with the selection, but adjust to keep on screen if needed.
  int x =
      std::min(caret_bounds_.x(), screen_work_area.right() - new_size.width() -
                                      kMakoRewritePadding);

  // Then, try to place the mako UI just under the first line of the selection.
  // Use a default caret height here since `caret bounds` is currently the
  // bounds of the entire selection, which may span several lines.
  // TODO: b/302043981 - Use the actual height of the first line.
  int y = caret_bounds_.y() + kDefaultCaretHeight + kMakoRewritePadding;
  // If that puts it offscreen, try placing it above the selection instead.
  if (y + new_size.height() + kMakoRewritePadding > screen_work_area.bottom()) {
    y = caret_bounds_.y() - kMakoRewritePadding - new_size.height();
  }

  // If it's still offscreen, place it at the bottom of the screen and adjust
  // the horizontal position to try to move it out of the way of the
  // selection.
  if (y < screen_work_area.y() + kMakoRewritePadding) {
    y = screen_work_area.bottom() - kMakoRewritePadding - new_size.height();
    // Place it at the right of the selection edge if there is space
    // (including padding), otherwise, place it to the left of the selection.
    x = screen_work_area.right() - caret_bounds_.x() >
                new_size.width() + 2 * kMakoRewritePadding
            ? caret_bounds_.x() + kMakoRewritePadding
            : caret_bounds_.x() - kMakoRewritePadding - new_size.width();
  }

  // Compute widget bounds, which includes the border and shadow around the main
  // contents. Then, adjust again to ensure the whole widget is onscreen.
  gfx::Rect widget_bounds({x, y}, new_size);
  widget_bounds.Inset(-GetBubbleFrameView()->bubble_border()->GetInsets());
  widget_bounds.AdjustToFit(screen_work_area);

  GetWidget()->SetBounds(widget_bounds);
}

bool MakoRewriteView::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.GetType() == content::NativeWebKeyboardEvent::Type::kRawKeyDown &&
      event.dom_key == ui::DomKey::ESCAPE) {
    return true;
  }

  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

BEGIN_METADATA(MakoRewriteView, WebUIBubbleDialogView)
END_METADATA

}  // namespace ash

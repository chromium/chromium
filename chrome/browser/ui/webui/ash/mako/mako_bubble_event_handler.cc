// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_bubble_event_handler.h"

#include "third_party/skia/include/core/SkRegion.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {

MakoBubbleEventHandler::MakoBubbleEventHandler(Delegate* delegate)
    : delegate_(delegate) {}

void MakoBubbleEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  ProcessPointerEvent(*event);
}

void MakoBubbleEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  ProcessPointerEvent(*event);
}

bool MakoBubbleEventHandler::get_dragging_for_testing() {
  return dragging_;
}

void MakoBubbleEventHandler::set_dragging_for_testing(bool dragging) {
  dragging_ = dragging;
}

void MakoBubbleEventHandler::set_original_bounds_in_screen_for_testing(
    gfx::Rect bounds) {
  original_bounds_in_screen_ = bounds;
}

void MakoBubbleEventHandler::set_original_pointer_pos_for_testing(
    gfx::Vector2d pos) {
  original_pointer_pos_ = pos;
}

bool MakoBubbleEventHandler::IsInSameDisplay(const gfx::Rect& original_bounds,
                                             const gfx::Rect& new_bounds) {
  display::Screen* screen = display::Screen::GetScreen();
  if (!screen) {
    return false;
  }
  display::Display original_display =
      screen->GetDisplayMatching(original_bounds);
  display::Display new_display = screen->GetDisplayMatching(new_bounds);
  return new_display.id() == original_display.id();
}

void MakoBubbleEventHandler::ProcessPointerEvent(ui::LocatedEvent& event) {
  if (!delegate_) {
    return;
  }

  const gfx::Rect bounds_in_screen = delegate_->GetWidgetBoundsInScreen();
  const std::optional<SkRegion> draggable_region =
      delegate_->GetDraggableRegion();
  const gfx::Vector2d pointer_pos(base::ClampFloor(event.x()),
                                  base::ClampFloor(event.y()));

  switch (event.type()) {
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_MOUSE_PRESSED:
      if (draggable_region.has_value() &&
          draggable_region->contains(pointer_pos.x(), pointer_pos.y())) {
        dragging_ = true;
        original_bounds_in_screen_ = bounds_in_screen;
        original_pointer_pos_ = gfx::Vector2d(
            /*x=*/bounds_in_screen.x() + pointer_pos.x(),
            /*y=*/bounds_in_screen.y() + pointer_pos.y());
      }
      break;
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_TOUCH_MOVED: {
      if (!dragging_) {
        break;
      }
      gfx::Rect new_bounds = original_bounds_in_screen_ +
                             bounds_in_screen.OffsetFromOrigin() + pointer_pos -
                             original_pointer_pos_;
      // If user moves mouse to another display while dragging, we see that
      // as a completion of dragging.
      if (!IsInSameDisplay(original_bounds_in_screen_, new_bounds)) {
        dragging_ = false;
        break;
      }
      delegate_->SetWidgetBoundsConstrained(new_bounds);
      event.SetHandled();
    } break;

    default:
      dragging_ = false;
      break;
  }
}

}  // namespace ash

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

namespace {

using State = MakoBubbleEventHandler::State;
using InitialState = MakoBubbleEventHandler::InitialState;
using DraggingState = MakoBubbleEventHandler::DraggingState;

bool IsInSameDisplay(const gfx::Rect& original_bounds,
                     const gfx::Rect& new_bounds) {
  display::Screen* screen = display::Screen::GetScreen();
  if (screen == nullptr) {
    return false;
  }
  display::Display original_display =
      screen->GetDisplayMatching(original_bounds);
  display::Display new_display = screen->GetDisplayMatching(new_bounds);
  return new_display.id() == original_display.id();
}

class StateProcessFunction {
 public:
  StateProcessFunction(ui::LocatedEvent* event,
                       MakoBubbleEventHandler::Delegate* delegate)
      : event_(event), delegate_(delegate) {}

  State operator()(const InitialState& s) {
    if (event_->type() == ui::ET_TOUCH_PRESSED ||
        event_->type() == ui::ET_MOUSE_PRESSED) {
      // Starts dragging.
      if (InDraggableRegion()) {
        MarkEventHandled();
        return DraggingState{
            .original_bounds_in_screen = GetBoundsInScreen(),
            .original_pointer_pos =
                GetBoundsInScreen().OffsetFromOrigin() + GetPointerPos(),
        };
      }
    }

    return InitialState{};
  }

  State operator()(const DraggingState& s) {
    // Keeps dragging.
    if (event_->type() == ui::ET_MOUSE_DRAGGED ||
        event_->type() == ui::ET_TOUCH_MOVED) {
      gfx::Rect new_bounds = s.original_bounds_in_screen +
                             GetBoundsInScreen().OffsetFromOrigin() +
                             GetPointerPos() - s.original_pointer_pos;

      // If user moves mouse to another display while dragging, we see that
      // as a completion of dragging.
      if (!IsInSameDisplay(s.original_bounds_in_screen, new_bounds)) {
        return InitialState{};
      }

      SetWidgetBoundsConstrained(new_bounds);
      MarkEventHandled();

      return s;
    }

    return InitialState{};
  }

 private:
  gfx::Vector2d GetPointerPos() const {
    return gfx::Vector2d(base::ClampFloor(event_->x()),
                         base::ClampFloor(event_->y()));
  }

  gfx::Rect GetBoundsInScreen() const {
    return delegate_->GetWidgetBoundsInScreen();
  }

  void SetWidgetBoundsConstrained(const gfx::Rect bounds) {
    delegate_->SetWidgetBoundsConstrained(bounds);
  }

  void MarkEventHandled() { event_->SetHandled(); }

  bool InDraggableRegion() {
    std::optional<SkRegion> draggable_region = delegate_->GetDraggableRegion();
    gfx::Vector2d pointer_pos = GetPointerPos();
    return draggable_region.has_value() &&
           draggable_region->contains(pointer_pos.x(), pointer_pos.y());
  }

  raw_ptr<ui::LocatedEvent> event_;
  raw_ptr<MakoBubbleEventHandler::Delegate> delegate_;
};

}  // namespace

MakoBubbleEventHandler::MakoBubbleEventHandler(Delegate* delegate)
    : delegate_(delegate) {}

void MakoBubbleEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  ProcessPointerEvent(*event);
}

void MakoBubbleEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  ProcessPointerEvent(*event);
}

MakoBubbleEventHandler::State MakoBubbleEventHandler::get_state_for_testing() {
  return state_;
}

void MakoBubbleEventHandler::set_state_for_testing(State s) {
  state_ = s;
}

void MakoBubbleEventHandler::ProcessPointerEvent(ui::LocatedEvent& event) {
  if (!delegate_) {
    return;
  }
  state_ = absl::visit(
      StateProcessFunction(/*event=*/&event, /*delegate=*/delegate_), state_);
}

}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_bubble_event_handler.h"

#include "ash/constants/ash_features.h"
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
using ResizingDirection = MakoBubbleEventHandler::ResizingDirection;
using ResizingState = MakoBubbleEventHandler::ResizingState;

constexpr int kResizingRegionWidth = 4;
constexpr int kMinWidgetHeight = 343;
constexpr int kMinWidgetWidth = 440;
constexpr int kWidgetCornerRadius = 20;
constexpr int kWidgetPadding = 4;

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

// Converts resizing direction to unit vector.
gfx::Vector2d MapResizingDirectionToVector(ResizingDirection direction) {
  switch (direction) {
    case ResizingDirection::kTop:
      return gfx::Vector2d(0, -1);
    case ResizingDirection::kBottom:
      return gfx::Vector2d(0, +1);
    case ResizingDirection::kLeft:
      return gfx::Vector2d(-1, 0);
    case ResizingDirection::kRight:
      return gfx::Vector2d(+1, 0);
    case ResizingDirection::kTopLeft:
      return gfx::Vector2d(-1, -1);
    case ResizingDirection::kTopRight:
      return gfx::Vector2d(+1, -1);
    case ResizingDirection::kBottomLeft:
      return gfx::Vector2d(-1, +1);
    case ResizingDirection::kBottomRight:
      return gfx::Vector2d(+1, +1);
    default:
      return gfx::Vector2d(0, 0);
  }
}

ui::mojom::CursorType MapResizingDirectionToCursorType(
    ResizingDirection direction) {
  switch (direction) {
    case ResizingDirection::kTop:
      return ui::mojom::CursorType::kNorthResize;
    case ResizingDirection::kBottom:
      return ui::mojom::CursorType::kSouthResize;
    case ResizingDirection::kLeft:
      return ui::mojom::CursorType::kWestResize;
    case ResizingDirection::kRight:
      return ui::mojom::CursorType::kEastResize;
    case ResizingDirection::kTopLeft:
      return ui::mojom::CursorType::kNorthWestResize;
    case ResizingDirection::kTopRight:
      return ui::mojom::CursorType::kNorthEastResize;
    case ResizingDirection::kBottomLeft:
      return ui::mojom::CursorType::kSouthWestResize;
    case ResizingDirection::kBottomRight:
      return ui::mojom::CursorType::kSouthEastResize;
    default:
      return ui::mojom::CursorType::kPointer;
  }
}

ResizingDirection ComputeResizingDirection(gfx::Rect bounds,
                                           gfx::Vector2d pointer_pos) {
  // Detect resizing on corners.
  if (pointer_pos.x() < (kWidgetCornerRadius + kWidgetPadding) &&
      pointer_pos.y() < (kWidgetCornerRadius + kWidgetPadding)) {
    return ResizingDirection::kTopLeft;
  }
  if (pointer_pos.x() >
          (bounds.width() - kWidgetCornerRadius - kWidgetPadding) &&
      pointer_pos.y() < (kWidgetCornerRadius + kWidgetPadding)) {
    return ResizingDirection::kTopRight;
  }
  if (pointer_pos.x() < (kWidgetCornerRadius + kWidgetPadding) &&
      pointer_pos.y() >
          (bounds.height() - kWidgetCornerRadius - kWidgetPadding)) {
    return ResizingDirection::kBottomLeft;
  }
  if (pointer_pos.x() >
          (bounds.width() - kWidgetCornerRadius - kWidgetPadding) &&
      pointer_pos.y() >
          (bounds.height() - kWidgetCornerRadius - kWidgetPadding)) {
    return ResizingDirection::kBottomRight;
  }

  // Detect resizing on edges.
  if (pointer_pos.x() < kResizingRegionWidth) {
    return ResizingDirection::kLeft;
  }
  if (pointer_pos.x() > bounds.width() - kResizingRegionWidth) {
    return ResizingDirection::kRight;
  }
  if (pointer_pos.y() < kResizingRegionWidth) {
    return ResizingDirection::kTop;
  }
  if (pointer_pos.y() > bounds.height() - kResizingRegionWidth) {
    return ResizingDirection::kBottom;
  }

  // Otherwise, it's not resizing.
  return ResizingDirection::kNone;
}

class StateProcessFunction {
 public:
  StateProcessFunction(ui::LocatedEvent* event,
                       MakoBubbleEventHandler::Delegate* delegate)
      : event_(event), delegate_(delegate) {}

  State operator()(const InitialState& s) {
    // Re-computes cursor type when touch point moves.
    if (event_->type() == ui::EventType::kTouchMoved ||
        event_->type() == ui::EventType::kMouseMoved) {
      if (delegate_->IsResizingEnabled()) {
        ResizingDirection direction = ComputeResizingDirection(
            /*bounds=*/GetBoundsInScreen(),
            /*pointer_pos=*/GetPointerPos());
        delegate_->SetCursor(
            ui::Cursor(MapResizingDirectionToCursorType(direction)));
      }

      return InitialState{};
    }

    if (event_->type() == ui::EventType::kTouchPressed ||
        event_->type() == ui::EventType::kMousePressed) {
      if (delegate_->IsResizingEnabled()) {
        ResizingDirection direction = ComputeResizingDirection(
            /*bounds=*/GetBoundsInScreen(),
            /*pointer_pos=*/GetPointerPos());

        // Starts resizing.
        if (direction != ResizingDirection::kNone) {
          MarkEventHandled();
          return ResizingState{
              .resizing_direction = direction,
              .original_bounds_in_screen = GetBoundsInScreen(),
              .original_pointer_pos =
                  GetBoundsInScreen().OffsetFromOrigin() + GetPointerPos(),
          };
        }
      }

      // Starts dragging.
      if (delegate_->IsDraggingEnabled() && InDraggableRegion()) {
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
    if (event_->type() == ui::EventType::kMouseDragged ||
        event_->type() == ui::EventType::kTouchMoved) {
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

  State operator()(const ResizingState& s) {
    // Keeps resizing.
    if (event_->type() == ui::EventType::kMouseDragged ||
        event_->type() == ui::EventType::kTouchMoved) {
      gfx::Vector2d resizing_unit =
          MapResizingDirectionToVector(s.resizing_direction);
      gfx::Vector2d pointer_delta = GetBoundsInScreen().OffsetFromOrigin() +
                                    GetPointerPos() - s.original_pointer_pos;

      int new_width = std::max(s.original_bounds_in_screen.width() +
                                   pointer_delta.x() * resizing_unit.x(),
                               kMinWidgetWidth);
      int new_height = std::max(s.original_bounds_in_screen.height() +
                                    pointer_delta.y() * resizing_unit.y(),
                                kMinWidgetHeight);

      int new_x = s.original_bounds_in_screen.x();
      int new_y = s.original_bounds_in_screen.y();

      if (resizing_unit.x() < 0) {
        new_x -= new_width - s.original_bounds_in_screen.width();
      }

      if (resizing_unit.y() < 0) {
        new_y -= new_height - s.original_bounds_in_screen.height();
      }

      SetWidgetBoundsConstrained(gfx::Rect(/*x=*/new_x, /*y=*/new_y,
                                           /*width=*/new_width,
                                           /*height=*/new_height));
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
    // The pointer pos here is based on MakoRewriteView window (padding
    // included) but the draggable region here is based on web contents. So we
    // need to add padding on pointer pos.
    gfx::Vector2d pointer_pos =
        GetPointerPos() + gfx::Vector2d(kWidgetPadding, kWidgetPadding);
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

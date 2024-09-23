// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_event_handler_aura.h"

#include "base/check_deref.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

WebUIBubbleEventHandlerAura::WebUIBubbleEventHandlerAura() = default;

WebUIBubbleEventHandlerAura::~WebUIBubbleEventHandlerAura() = default;

void WebUIBubbleEventHandlerAura::OnMouseEvent(ui::MouseEvent* event) {
  ProcessLocatedEvent(event);
}

void WebUIBubbleEventHandlerAura::OnGestureEvent(ui::GestureEvent* event) {
  ProcessLocatedEvent(event);
}

bool WebUIBubbleEventHandlerAura::IsInSameDisplay(
    const gfx::Rect& original_bounds,
    const gfx::Rect& new_bounds) {
  const display::Screen* screen = display::Screen::GetScreen();
  CHECK(screen);
  const display::Display original_display =
      screen->GetDisplayMatching(original_bounds);
  const display::Display new_display = screen->GetDisplayMatching(new_bounds);
  return new_display.id() == original_display.id();
}

void WebUIBubbleEventHandlerAura::ProcessLocatedEvent(ui::LocatedEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());

  // Touch drags must be handled via gesture events to ensure events touch
  // inputs been correctly interpreted by the gesture recognizer.
  switch (event->type()) {
    case ui::EventType::kGestureScrollBegin:
    case ui::EventType::kMousePressed: {
      // Only start drags on bubble caption regions.
      if (target->delegate()->GetNonClientComponent(event->location()) ==
          HTCAPTION) {
        dragging_ = true;
        initial_bounds_ = target->GetBoundsInScreen();
        initial_offset_ =
            gfx::ToFlooredVector2d(event->location().OffsetFromOrigin());
      }
      break;
    }
    case ui::EventType::kGestureScrollUpdate:
    case ui::EventType::kMouseDragged: {
      if (!dragging_) {
        break;
      }
      gfx::Point new_position = target->GetBoundsInScreen().origin();
      new_position.Offset(base::ClampFloor(event->x()),
                          base::ClampFloor(event->y()));
      new_position -= initial_offset_;

      gfx::Rect new_bounds = initial_bounds_;
      new_bounds.set_origin(new_position);

      // Constrain bubble dragging to its source display.
      if (!IsInSameDisplay(initial_bounds_, new_bounds)) {
        dragging_ = false;
        break;
      }

      target->SetBoundsInScreen(new_bounds,
                                CHECK_DEREF(display::Screen::GetScreen())
                                    .GetDisplayMatching(new_bounds));
      event->SetHandled();
      break;
    }
    default:
      dragging_ = false;
      break;
  }
}

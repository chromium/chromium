// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_event_handler_aura.h"

#include "base/check.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

namespace {

gfx::Point GetEventScreenPoint(const ui::LocatedEvent* event,
                               views::Widget* widget) {
  views::View* root_view = widget->GetRootView();
  return root_view
             ? views::View::ConvertPointToScreen(root_view, event->location())
             : event->root_location();
}

}  // namespace

OmniboxEverywhereEventHandlerAura::OmniboxEverywhereEventHandlerAura(
    OmniboxEverywhereUIManager& ui_manager)
    : ui_manager_(ui_manager) {}

OmniboxEverywhereEventHandlerAura::~OmniboxEverywhereEventHandlerAura() =
    default;

bool OmniboxEverywhereEventHandlerAura::IsPointInDraggableRegion(
    const gfx::Point& point) const {
  return ui_manager_->IsPointInDraggableRegion(point);
}

// TODO(crbug.com/532200145): Support touch gesture dragging via
// ui::GestureEvent (similar to WebUIBubbleEventHandlerAura).
void OmniboxEverywhereEventHandlerAura::OnMouseEvent(ui::MouseEvent* event) {
  views::Widget* widget = ui_manager_->widget();
  if (!widget) {
    return;
  }

  switch (event->type()) {
    case ui::EventType::kMousePressed: {
      if (event->IsOnlyLeftMouseButton()) {
        gfx::Point cursor_screen = GetEventScreenPoint(event, widget);
        gfx::Point point_in_contents = cursor_screen;
        if (views::View* contents_view =
                ui_manager_->widget_delegate()->GetContentsView()) {
          views::View::ConvertPointFromScreen(contents_view,
                                              &point_in_contents);
          if (IsPointInDraggableRegion(point_in_contents)) {
            drag_init_point_screen_ = cursor_screen;
          }
        }
      }
      break;
    }
    case ui::EventType::kMouseDragged: {
      if (drag_init_point_screen_.has_value()) {
        gfx::Point cursor_screen = GetEventScreenPoint(event, widget);
        gfx::Vector2d delta = cursor_screen - *drag_init_point_screen_;
        if (views::View::ExceededDragThreshold(delta)) {
          drag_init_point_screen_.reset();
          event->SetHandled();
          gfx::Vector2d drag_offset =
              widget->GetWindowBoundsInScreen().origin() - cursor_screen;
          widget->RunMoveLoop(drag_offset,
                              views::Widget::MoveLoopSource::kMouse,
                              views::Widget::MoveLoopEscapeBehavior::kDontHide);
        }
      }
      break;
    }
    case ui::EventType::kMouseReleased:
    case ui::EventType::kMouseCaptureChanged: {
      drag_init_point_screen_.reset();
      break;
    }
    default:
      break;
  }
}

}  // namespace omnibox_everywhere

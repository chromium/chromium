// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_event_handler_aura.h"

#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"
#elif BUILDFLAG(IS_LINUX)
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"
#endif

namespace omnibox_everywhere {

namespace {

gfx::Point GetEventScreenPoint(const ui::LocatedEvent* event,
                               views::Widget* widget) {
  gfx::Point location = event->location();
  if (auto* target_window = static_cast<aura::Window*>(event->target());
      target_window && widget->GetNativeView()) {
    aura::Window::ConvertPointToTarget(target_window, widget->GetNativeView(),
                                       &location);
  }
  views::View* root_view = widget->GetRootView();
  return root_view ? views::View::ConvertPointToScreen(root_view, location)
                   : event->root_location();
}

base::ScopedClosureRunner SuppressDataDragsForWindowMove() {
#if BUILDFLAG(IS_WIN)
  return views::DesktopDragDropClientWin::ScopedSuppressForWindowMove();
#elif BUILDFLAG(IS_LINUX)
  return views::DesktopDragDropClientOzone::ScopedSuppressForWindowMove();
#else
  return base::ScopedClosureRunner();
#endif
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
    ResetDragState();
    return;
  }

  switch (event->type()) {
    case ui::EventType::kMousePressed: {
      ResetDragState();
      if (event->IsOnlyLeftMouseButton()) {
        gfx::Point cursor_screen = GetEventScreenPoint(event, widget);
        gfx::Point point_in_contents = cursor_screen;
        if (views::View* contents_view =
                ui_manager_->widget_delegate()->GetContentsView()) {
          views::View::ConvertPointFromScreen(contents_view,
                                              &point_in_contents);
          if (IsPointInDraggableRegion(point_in_contents)) {
            drag_init_point_screen_ = cursor_screen;
            window_move_drag_suppression_ = SuppressDataDragsForWindowMove();
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
          // Keep data drags suppressed for the whole move loop by moving the
          // scope onto the stack. It must not stay in the member because:
          // - On Windows, RunMoveLoop() calls ::ReleaseCapture() before the
          //   native move loop starts, which delivers kMouseCaptureChanged
          //   (and so ResetDragState()) before IsInNativeMoveResizeLoop()
          //   starts blocking drags.
          // - `this` may be destroyed while RunMoveLoop() is on the stack.
          base::ScopedClosureRunner drag_suppression =
              std::move(window_move_drag_suppression_);
          event->SetHandled();
          gfx::Vector2d drag_offset =
              widget->GetWindowBoundsInScreen().origin() - cursor_screen;
          ui_manager_->OnWidgetUserDragStarted(widget);
          base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();
          auto weak_this = weak_factory_.GetWeakPtr();
          widget->RunMoveLoop(drag_offset,
                              views::Widget::MoveLoopSource::kMouse,
                              views::Widget::MoveLoopEscapeBehavior::kDontHide);
          if (weak_this && weak_widget) {
            ui_manager_->OnWidgetUserDragEnded(weak_widget.get());
          }
        }
      }
      break;
    }
    case ui::EventType::kMouseReleased:
    case ui::EventType::kMouseCaptureChanged: {
      ResetDragState();
      break;
    }
    case ui::EventType::kMouseMoved: {
      // Aura only sends kMouseMoved while no mouse button is down, so the
      // release that ended any pending hold was missed. Other events, such as
      // kMousewheel, can arrive while the button is still held and must not
      // end the hold. (kMouseExited is not used either: on Windows it comes
      // from WM_MOUSELEAVE, which carries no button state.)
      ResetDragState();
      break;
    }
    default:
      break;
  }
}

void OmniboxEverywhereEventHandlerAura::ResetDragState() {
  drag_init_point_screen_.reset();
  window_move_drag_suppression_.RunAndReset();
}

}  // namespace omnibox_everywhere

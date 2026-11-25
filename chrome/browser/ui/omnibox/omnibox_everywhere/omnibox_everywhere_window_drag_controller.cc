// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_window_drag_controller.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/omnibox/omnibox_everywhere/mac_window_util.h"
#elif BUILDFLAG(IS_WIN)
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"
#elif BUILDFLAG(IS_LINUX)
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_ozone.h"
#endif

namespace omnibox_everywhere {

namespace {

base::ScopedClosureRunner SuppressDataDragsForWindowMove() {
#if BUILDFLAG(IS_WIN)
  return views::DesktopDragDropClientWin::ScopedSuppressForWindowMove();
#elif BUILDFLAG(IS_LINUX)
  return views::DesktopDragDropClientOzone::ScopedSuppressForWindowMove();
#else
  return base::ScopedClosureRunner();
#endif
}

// Returns true for a press of only the left button that isn't a secondary
// click.
bool IsPlainLeftPress(const blink::WebMouseEvent& event) {
  if (event.button != blink::WebMouseEvent::Button::kLeft ||
      (event.GetModifiers() & (blink::WebInputEvent::kMiddleButtonDown |
                               blink::WebInputEvent::kRightButtonDown |
                               blink::WebInputEvent::kBackButtonDown |
                               blink::WebInputEvent::kForwardButtonDown)) !=
          0) {
    return false;
  }
#if BUILDFLAG(IS_MAC)
  // Ctrl+click is a secondary click on macOS.
  if ((event.GetModifiers() & blink::WebInputEvent::kControlKey) != 0) {
    return false;
  }
#endif
  return true;
}

}  // namespace

OmniboxEverywhereWindowDragController::OmniboxEverywhereWindowDragController(
    Delegate& delegate)
    : delegate_(delegate) {}

OmniboxEverywhereWindowDragController::
    ~OmniboxEverywhereWindowDragController() = default;

// TODO(crbug.com/532200145): Support touch gesture dragging via
// `PreHandleGestureEvent()`.
bool OmniboxEverywhereWindowDragController::PreHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  if (replaying_press_ || !delegate_->GetWindowDragWidget()) {
    return false;
  }
  if (delegate_->IsWindowDragging()) {
    // Hide mouse events from Blink while the window is dragged, including the
    // release that ends the drag, since Blink never saw its press.
    if (event.GetType() == blink::WebInputEvent::Type::kMouseUp &&
        event.button == blink::WebMouseEvent::Button::kLeft) {
      Reset();
    }
    return true;
  }
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kMouseDown:
      return HandleMouseDown(event);
    case blink::WebInputEvent::Type::kMouseMove:
      return HandleMouseMove(event);
    case blink::WebInputEvent::Type::kMouseUp:
      return HandleMouseUp(event);
    default:
      return false;
  }
}

bool OmniboxEverywhereWindowDragController::HandleMouseDown(
    const blink::WebMouseEvent& event) {
  if (held_press_ && !IsPlainLeftPress(event)) {
    // Another button was pressed, so it isn't a drag. Send the held press
    // first.
    ReplayHeldPress();
  }
  Reset();
  if (!IsPlainLeftPress(event) ||
      !delegate_->IsPointInDraggableRegion(
          gfx::ToFlooredPoint(event.PositionInWidget())) ||
      !PrepareForWindowDrag()) {
    return false;
  }
  // Hold the press until it's known to be a click or a drag.
  held_press_ = event;
  window_move_drag_suppression_ = SuppressDataDragsForWindowMove();
  return true;
}

bool OmniboxEverywhereWindowDragController::HandleMouseMove(
    const blink::WebMouseEvent& event) {
  if (!held_press_) {
    return false;
  }
  if (event.button == blink::WebMouseEvent::Button::kNoButton) {
    // The button was released without a mouse up reaching us, e.g. while
    // another window had mouse capture. Drop the press.
    Reset();
    return false;
  }
  const gfx::Point press_point_screen =
      gfx::ToFlooredPoint(held_press_->PositionInScreen());
  if (!views::View::ExceededDragThreshold(
          gfx::ToFlooredPoint(event.PositionInScreen()) - press_point_screen)) {
    // Blink hasn't seen the press, so hide moves from it too.
    return true;
  }
  // `Widget::RunMoveLoop()` expects the press point relative to the window
  // origin.
  const gfx::Vector2d drag_offset =
      press_point_screen -
      delegate_->GetWindowDragWidget()->GetWindowBoundsInScreen().origin();
  held_press_.reset();
  delegate_->OnWindowDragStarted();
  // Post the move loop so it doesn't run inside mouse event dispatch.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OmniboxEverywhereWindowDragController::StartWindowMoveLoop,
          move_loop_weak_factory_.GetWeakPtr(), drag_offset));
  return true;
}

bool OmniboxEverywhereWindowDragController::HandleMouseUp(
    const blink::WebMouseEvent& event) {
  if (held_press_) {
    // A click: send the held press, then let the release follow it.
    ReplayHeldPress();
  }
  return false;
}

void OmniboxEverywhereWindowDragController::ReplayHeldPress() {
  const blink::WebMouseEvent press = *held_press_;
  Reset();
  base::AutoReset<bool> replaying_press(&replaying_press_, true);
  delegate_->ForwardMouseEventToRenderer(press);
}

void OmniboxEverywhereWindowDragController::Reset() {
  held_press_.reset();
  window_move_drag_suppression_.RunAndReset();
  if (move_loop_weak_factory_.HasWeakPtrs()) {
    // Cancel a move loop that was posted but hasn't started.
    move_loop_weak_factory_.InvalidateWeakPtrs();
    delegate_->OnWindowDragEnded();
  }
}

bool OmniboxEverywhereWindowDragController::PrepareForWindowDrag() {
  if (!delegate_->CanStartWindowDrag()) {
    return false;
  }
  // Don't drag while any menu is open or fading out.
  if (views::MenuController::GetActiveInstance()) {
    return false;
  }
#if BUILDFLAG(IS_MAC)
  return CanStartWindowMoveLoopOnMac();
#else
  return true;
#endif
}

void OmniboxEverywhereWindowDragController::StartWindowMoveLoop(
    const gfx::Vector2d& drag_offset) {
  move_loop_weak_factory_.InvalidateWeakPtrs();
  // Keep data drags suppressed for the whole move loop (crbug.com/576129731).
  base::ScopedClosureRunner drag_suppression =
      std::move(window_move_drag_suppression_);
  views::Widget* widget = delegate_->GetWindowDragWidget();
  if (widget && PrepareForWindowDrag()) {
    auto weak_this = weak_factory_.GetWeakPtr();
    base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
    widget->RunMoveLoop(drag_offset, views::Widget::MoveLoopSource::kMouse,
                        views::Widget::MoveLoopEscapeBehavior::kDontHide);
    if (!weak_this || !weak_widget) {
      return;
    }
  }
  delegate_->OnWindowDragEnded();
}

}  // namespace omnibox_everywhere

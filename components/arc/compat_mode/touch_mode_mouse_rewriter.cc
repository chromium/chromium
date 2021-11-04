// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/touch_mode_mouse_rewriter.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/events/base_event_utils.h"

namespace arc {

namespace {
// In Android, the default long press threshold is 500ms.
constexpr base::TimeDelta kLongPressInterval = base::Milliseconds(700);
}  // namespace

TouchModeMouseRewriter::TouchModeMouseRewriter() = default;
TouchModeMouseRewriter::~TouchModeMouseRewriter() = default;

ui::EventDispatchDetails TouchModeMouseRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!event.IsMouseEvent())
    return SendEvent(continuation, &event);

  const ui::MouseEvent& mouse_event = *event.AsMouseEvent();
  if (mouse_event.IsRightMouseButton()) {
    // 1. If there is already an ongoing simulated long press, discard the
    //    subsequent right click.
    // 2. If the left button is currently pressed, discard the right click.
    // 3. Discard events that is not a right press.
    if (release_event_scheduled_ || left_pressed_ ||
        mouse_event.type() != ui::ET_MOUSE_PRESSED) {
      return DiscardEvent(continuation);
    }
    // Schedule the release event after |kLongPressInterval|.
    release_event_scheduled_ = true;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TouchModeMouseRewriter::SendReleaseEvent,
                       weak_ptr_factory_.GetWeakPtr(), mouse_event,
                       continuation),
        kLongPressInterval);
    // Send the press event now.
    ui::MouseEvent press_event(
        ui::ET_MOUSE_PRESSED, mouse_event.location(),
        mouse_event.root_location(), mouse_event.time_stamp(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    return SendEvent(continuation, &press_event);
  } else if (mouse_event.IsLeftMouseButton()) {
    if (mouse_event.type() == ui::ET_MOUSE_PRESSED)
      left_pressed_ = true;
    else if (mouse_event.type() == ui::ET_MOUSE_RELEASED)
      left_pressed_ = false;
    // Discard a release event that corresponds to a previously discarded press
    // event.
    if (discard_next_left_release_ &&
        mouse_event.type() == ui::ET_MOUSE_RELEASED) {
      discard_next_left_release_ = false;
      return DiscardEvent(continuation);
    }
    // Discard the left click if there is an ongoing simulated long press.
    if (release_event_scheduled_) {
      if (mouse_event.type() == ui::ET_MOUSE_PRESSED)
        discard_next_left_release_ = true;
      return DiscardEvent(continuation);
    }
    return SendEvent(continuation, &event);
  }
  return SendEvent(continuation, &event);
}

void TouchModeMouseRewriter::SendReleaseEvent(
    const ui::MouseEvent& original_event,
    const Continuation continuation) {
  release_event_scheduled_ = false;
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, original_event.location(),
                               original_event.root_location(),
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ignore_result(SendEvent(continuation, &release_event));
}

}  // namespace arc

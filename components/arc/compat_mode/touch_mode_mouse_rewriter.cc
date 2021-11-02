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
  if (!event.IsMouseEvent()) {
    return SendEvent(continuation, &event);
  }
  const ui::MouseEvent& mouse_event = *event.AsMouseEvent();
  if (!mouse_event.IsRightMouseButton()) {
    return SendEvent(continuation, &event);
  }
  if (mouse_event.type() != ui::ET_MOUSE_PRESSED) {
    return DiscardEvent(continuation);
  }
  // Schedule the release event after |kLongPressInterval|.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TouchModeMouseRewriter::SendReleaseEvent,
                     weak_ptr_factory_.GetWeakPtr(), mouse_event, continuation),
      kLongPressInterval);
  // Send the press event now.
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, mouse_event.location(),
                             mouse_event.root_location(),
                             mouse_event.time_stamp(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  return SendEvent(continuation, &press_event);
}

void TouchModeMouseRewriter::SendReleaseEvent(
    const ui::MouseEvent& original_event,
    const Continuation continuation) {
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, original_event.location(),
                               original_event.root_location(),
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  ignore_result(SendEvent(continuation, &release_event));
}

}  // namespace arc

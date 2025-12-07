// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/input_events_blocker.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "ui/events/event.h"

namespace ash {

InputEventsBlocker::InputEventsBlocker() {
  Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
  VLOG(1) << "InputEventsBlocker " << this << " created.";
}

InputEventsBlocker::~InputEventsBlocker() {
  if (Shell::HasInstance()) {
    Shell::Get()->RemovePreTargetHandler(this);
  }
  VLOG(1) << "InputEventsBlocker " << this << " destroyed.";
}

void InputEventsBlocker::OnKeyEvent(ui::KeyEvent* event) {
  event->StopPropagation();
}

void InputEventsBlocker::OnMouseEvent(ui::MouseEvent* event) {
  event->StopPropagation();
}

void InputEventsBlocker::OnTouchEvent(ui::TouchEvent* event) {
  event->StopPropagation();
}

void InputEventsBlocker::OnGestureEvent(ui::GestureEvent* event) {
  event->StopPropagation();
}

}  // namespace ash

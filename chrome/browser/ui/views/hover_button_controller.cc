// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hover_button_controller.h"

#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller_delegate.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

HoverButtonController::HoverButtonController(
    views::Button* button,
    views::ButtonListener* listener,
    std::unique_ptr<views::ButtonControllerDelegate> delegate)
    : ButtonController(button, std::move(delegate)), listener_(listener) {
  set_notify_action(views::ButtonController::NotifyAction::kOnRelease);
}

HoverButtonController::~HoverButtonController() = default;

bool HoverButtonController::OnKeyPressed(const ui::KeyEvent& event) {
  if (!listener_)
    return false;

  switch (event.key_code()) {
    case ui::VKEY_SPACE:
    case ui::VKEY_RETURN:
      listener_->ButtonPressed(button(), event);
      return true;
    default:
      break;
  }
  return false;
}

bool HoverButtonController::OnMousePressed(const ui::MouseEvent& event) {
  DCHECK(notify_action() == views::ButtonController::NotifyAction::kOnRelease);
  if (button()->request_focus_on_press())
    button()->RequestFocus();
  if (listener_) {
    button()->AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED,
                             ui::LocatedEvent::FromIfValid(&event));
  } else {
    button()->AnimateInkDrop(views::InkDropState::HIDDEN,
                             ui::LocatedEvent::FromIfValid(&event));
  }
  return true;
}

void HoverButtonController::OnMouseReleased(const ui::MouseEvent& event) {
  DCHECK(notify_action() == views::ButtonController::NotifyAction::kOnRelease);
  if (button()->state() != views::Button::STATE_DISABLED &&
      delegate()->IsTriggerableEvent(event) &&
      button()->HitTestPoint(event.location()) && !delegate()->InDrag()) {
    if (listener_)
      listener_->ButtonPressed(button(), event);
  } else {
    button()->AnimateInkDrop(views::InkDropState::HIDDEN, &event);
    ButtonController::OnMouseReleased(event);
  }
}

void HoverButtonController::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    if (listener_)
      listener_->ButtonPressed(button(), *event);
    button()->SetState(views::Button::STATE_NORMAL);
  } else {
    ButtonController::OnGestureEvent(event);
  }
}

// Copyright 2019 The Chromium Authors
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
    views::Button::PressedCallback callback,
    std::unique_ptr<views::ButtonControllerDelegate> delegate)
    : ButtonController(button, std::move(delegate)),
      callback_(std::move(callback)) {
  set_notify_action(views::ButtonController::NotifyAction::kOnRelease);
}

HoverButtonController::~HoverButtonController() = default;

bool HoverButtonController::OnKeyPressed(const ui::KeyEvent& event) {
  const bool pressed = callback_ && ((event.key_code() == ui::VKEY_SPACE) ||
                                     (event.key_code() == ui::VKEY_RETURN));
  if (pressed)
    callback_.Run(event);
  return pressed;
}

bool HoverButtonController::OnMousePressed(const ui::MouseEvent& event) {
  DCHECK(notify_action() == views::ButtonController::NotifyAction::kOnRelease);
  if (button()->GetRequestFocusOnPress())
    button()->RequestFocus();
  if (callback_) {
    views::InkDrop::Get(button())->AnimateToState(
        views::InkDropState::ACTION_PENDING,
        ui::LocatedEvent::FromIfValid(&event));
  } else {
    views::InkDrop::Get(button())->AnimateToState(
        views::InkDropState::HIDDEN, ui::LocatedEvent::FromIfValid(&event));
  }
  return true;
}

void HoverButtonController::OnMouseReleased(const ui::MouseEvent& event) {
  DCHECK(notify_action() == views::ButtonController::NotifyAction::kOnRelease);
  views::InkDrop::Get(button())->AnimateToState(views::InkDropState::HIDDEN,
                                                &event);
  if (button()->GetState() != views::Button::STATE_DISABLED &&
      delegate()->IsTriggerableEvent(event) &&
      button()->HitTestPoint(event.location()) && !delegate()->InDrag()) {
    if (callback_)
      callback_.Run(event);
  } else {
    ButtonController::OnMouseReleased(event);
  }
}

void HoverButtonController::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    button()->SetState(views::Button::STATE_NORMAL);
    if (callback_)
      callback_.Run(*event);
  } else {
    ButtonController::OnGestureEvent(event);
  }
}

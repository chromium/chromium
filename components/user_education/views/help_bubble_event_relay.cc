// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_event_relay.h"

#include "base/functional/bind.h"
#include "components/user_education/views/help_bubble_view.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace user_education {

namespace {

bool IsInButton(const gfx::Point& screen_coords, const views::Button* button) {
  return button && button->HitTestPoint(views::View::ConvertPointFromScreen(
                       button, screen_coords));
}

}  // namespace

HelpBubbleEventRelay::HelpBubbleEventRelay() = default;
HelpBubbleEventRelay::~HelpBubbleEventRelay() = default;

void HelpBubbleEventRelay::Init(HelpBubbleView* help_bubble) {
  CHECK(!help_bubble_);
  help_bubble_ = help_bubble;
}

views::Button* HelpBubbleEventRelay::GetButtonAt(
    const gfx::Point& screen_coords) const {
  if (!help_bubble_) {
    return nullptr;
  }
  if (IsInButton(screen_coords, help_bubble_->close_button_)) {
    return help_bubble_->close_button_;
  }
  if (IsInButton(screen_coords, help_bubble_->default_button_)) {
    return help_bubble_->default_button_;
  }
  for (views::MdTextButton* const button : help_bubble_->non_default_buttons_) {
    if (IsInButton(screen_coords, button)) {
      return button;
    }
  }
  return nullptr;
}

bool HelpBubbleEventRelay::OnEvent(const ui::LocatedEvent& event,
                                   const gfx::Point& screen_coords) {
  if (!help_bubble_) {
    return false;
  }

  const views::Widget* const widget = help_bubble_->GetWidget();
  if (!widget || !widget->GetWindowBoundsInScreen().Contains(screen_coords)) {
    return false;
  }

  views::Button* const target_button = GetButtonAt(screen_coords);
  const gfx::Point target_point =
      target_button
          ? views::View::ConvertPointFromScreen(target_button, screen_coords)
          : gfx::Point();

  switch (event.type()) {
    // Pass mouse events on to the button as normal.
    case ui::EventType::kMousePressed:
      if (target_button) {
        auto* const mouse_event = event.AsMouseEvent();
        target_button->OnMousePressed(ui::MouseEvent(
            ui::EventType::kMousePressed, gfx::PointF(target_point),
            gfx::PointF(screen_coords), mouse_event->time_stamp(),
            mouse_event->flags(), mouse_event->changed_button_flags()));
        sent_click_ = true;
      }
      break;
    case ui::EventType::kMouseReleased:
      if (target_button) {
        auto* const mouse_event = event.AsMouseEvent();
        target_button->OnMouseReleased(ui::MouseEvent(
            ui::EventType::kMouseReleased, gfx::PointF(target_point),
            gfx::PointF(screen_coords), mouse_event->time_stamp(),
            mouse_event->flags(), mouse_event->changed_button_flags()));
      }
      break;

    // Touch events are not processed directly by Views; they are typically
    // converted to something else. So, convert them to mouse clicks for the
    // purpose of pressing buttons.
    case ui::EventType::kTouchPressed:
      if (target_button) {
        auto* const touch_event = event.AsTouchEvent();
        target_button->OnMousePressed(ui::MouseEvent(
            ui::EventType::kMousePressed, gfx::PointF(target_point),
            gfx::PointF(screen_coords), touch_event->time_stamp(),
            touch_event->flags() | ui::EF_LEFT_MOUSE_BUTTON | ui::EF_FROM_TOUCH,
            ui::EF_LEFT_MOUSE_BUTTON));
        sent_click_ = true;
      }
      break;
    case ui::EventType::kTouchReleased:
      if (target_button) {
        auto* const touch_event = event.AsTouchEvent();
        target_button->OnMouseReleased(ui::MouseEvent(
            ui::EventType::kMouseReleased, gfx::PointF(target_point),
            gfx::PointF(screen_coords), touch_event->time_stamp(),
            touch_event->flags() | ui::EF_LEFT_MOUSE_BUTTON | ui::EF_FROM_TOUCH,
            ui::EF_LEFT_MOUSE_BUTTON));
      }
      break;

    // If a gesture is received, forward it as-is.
    case ui::EventType::kGestureTap:
      if (target_button) {
        auto* const gesture = event.AsGestureEvent();
        ui::GestureEvent tap(gesture->x(), gesture->y(), gesture->flags(),
                             gesture->time_stamp(), gesture->details(),
                             gesture->unique_touch_event_id());
        target_button->button_controller()->OnGestureEvent(&tap);
        sent_click_ = true;
      }
      break;

    // Mouse moves could be routed through the inkdrop controller but it's
    // easier to just set hovered state directly.
    case ui::EventType::kMouseMoved:
      if (target_button != hovered_button_) {
        if (hovered_button_) {
          views::InkDrop* const ink_drop =
              views::InkDrop::Get(hovered_button_)->GetInkDrop();
          if (ink_drop) {
            ink_drop->SetHovered(false);
          }
        }
        if (target_button) {
          views::InkDrop* const ink_drop =
              views::InkDrop::Get(target_button)->GetInkDrop();
          if (ink_drop) {
            ink_drop->SetHovered(true);
          }
        }
        hovered_button_ = target_button;
      }
      break;

    // For cases where the help bubble overlaps the target area, the mouse
    // exiting should result in un-hovering the current button.
    case ui::EventType::kMouseExited:
      if (ShouldUnHoverOnMouseExit() && hovered_button_) {
        if (views::InkDrop* const ink_drop =
                views::InkDrop::Get(hovered_button_)->GetInkDrop()) {
          ink_drop->SetHovered(false);
        }
        hovered_button_ = nullptr;
      }
      return false;

    default:
      return false;
  }

  return true;
}

void HelpBubbleEventRelay::OnConnectionLost() {
  if (!sent_click_ && help_bubble_ && help_bubble_->GetWidget() &&
      !help_bubble_->GetWidget()->IsClosed()) {
    help_bubble_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kLostFocus);
  }
}

namespace internal {

MenuHelpBubbleEventProcessor::MenuHelpBubbleEventProcessor(
    views::MenuItemView* menu_item)
    : callback_handle_(menu_item->GetMenuController()->AddAnnotationCallback(
          base::BindRepeating(&MenuHelpBubbleEventProcessor::OnEvent,
                              base::Unretained(this)))) {}

MenuHelpBubbleEventProcessor::~MenuHelpBubbleEventProcessor() = default;

bool MenuHelpBubbleEventProcessor::OnEvent(const ui::LocatedEvent& event) {
  return HelpBubbleEventRelay::OnEvent(event, event.root_location());
}

bool MenuHelpBubbleEventProcessor::ShouldHelpBubbleProcessEvents() const {
  return true;
}

bool MenuHelpBubbleEventProcessor::ShouldUnHoverOnMouseExit() const {
  return false;
}

}  // namespace internal

}  // namespace user_education

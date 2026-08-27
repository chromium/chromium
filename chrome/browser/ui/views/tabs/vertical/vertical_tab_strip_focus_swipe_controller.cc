// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_focus_swipe_controller.h"

#include <cmath>

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "ui/events/event.h"

VerticalTabStripFocusSwipeController::VerticalTabStripFocusSwipeController(
    VerticalTabStripRegionView* region_view)
    : region_view_(region_view) {
  CHECK(region_view_);
  region_view_->AddPreTargetHandler(this);
}

VerticalTabStripFocusSwipeController::~VerticalTabStripFocusSwipeController() {
  region_view_->RemovePreTargetHandler(this);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void VerticalTabStripFocusSwipeController::OnMouseEvent(ui::MouseEvent* event) {
  if (!base::FeatureList::IsEnabled(features::kTabGroupsFocusing) ||
      !region_view_ || region_view_->IsDragging() ||
      !event->IsMouseWheelEvent()) {
    return;
  }

  ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
  ProcessSwipeOffset(static_cast<float>(wheel_event->x_offset()),
                     static_cast<float>(wheel_event->y_offset()), wheel_event);
}
#elif BUILDFLAG(IS_MAC)
void VerticalTabStripFocusSwipeController::OnScrollEvent(
    ui::ScrollEvent* event) {
  if (!base::FeatureList::IsEnabled(features::kTabGroupsFocusing) ||
      !region_view_ || region_view_->IsDragging()) {
    return;
  }

  // Handle fling cancellation and stream resets.
  if (event->type() == ui::EventType::kScrollFlingStart ||
      event->type() == ui::EventType::kScrollFlingCancel) {
    if (gesture_state_ == GestureState::kHorizontalSwipe) {
      event->SetHandled();
      event->StopPropagation();
    }
    ResetGesture();
    return;
  }

  // Ignore momentum scroll events completely for triggering focus
  // mode rotations. If locked into a horizontal swipe, consume momentum events
  // until the momentum stream ends.
  if (event->momentum_phase() != ui::EventMomentumPhase::NONE) {
    if (gesture_state_ == GestureState::kHorizontalSwipe) {
      event->SetHandled();
      event->StopPropagation();
    }
    if (event->momentum_phase() == ui::EventMomentumPhase::END) {
      ResetGesture();
    }
    return;
  }

  if (event->scroll_event_phase() == ui::ScrollEventPhase::kBegan) {
    ResetGesture();
  }

  if (event->scroll_event_phase() == ui::ScrollEventPhase::kEnd) {
    if (gesture_state_ == GestureState::kHorizontalSwipe) {
      event->SetHandled();
      event->StopPropagation();
    }
    ResetGesture();
    return;
  }

  ProcessSwipeOffset(event->x_offset(), event->y_offset(), event);
}
#endif

void VerticalTabStripFocusSwipeController::ProcessSwipeOffset(
    float x_offset,
    float y_offset,
    ui::Event* event) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_event_time_.is_null() &&
      (now - last_event_time_) > kGestureResetTimeout) {
    ResetGesture();
  }
  last_event_time_ = now;

  // If we are already handling a vertical swipe, ignore everything.
  if (gesture_state_ == GestureState::kVerticalPassthrough) {
    return;
  }

  cumulative_x_ += x_offset;
  cumulative_y_ += y_offset;

  // Lock the axis as soon as we cross the axis lock distance threshold.
  if (gesture_state_ == GestureState::kNone) {
    if (std::abs(cumulative_y_) >= kAxisLockThreshold &&
        std::abs(cumulative_y_) > std::abs(cumulative_x_)) {
      gesture_state_ = GestureState::kVerticalPassthrough;
      return;
    }

    if (std::abs(cumulative_x_) >= kAxisLockThreshold &&
        std::abs(cumulative_x_) > std::abs(cumulative_y_)) {
      gesture_state_ = GestureState::kHorizontalSwipe;
    }
  }

  // Handle horizontal swipe. Consume events in the horizontal direction.
  if (gesture_state_ == GestureState::kHorizontalSwipe) {
    event->SetHandled();
    event->StopPropagation();

    // Trigger exactly once per physical swipe gesture. Subsequent events in
    // the same touch are consumed until fingers lift or gesture times out.
    if (!has_triggered_in_current_gesture_ &&
        std::abs(cumulative_x_) >= kSwipeThreshold) {
      bool forward = cumulative_x_ < 0;
      if (base::i18n::IsRTL()) {
        forward = !forward;
      }
      has_triggered_in_current_gesture_ = true;
      RotateTabGroupFocus(forward);
    }
  }
}

void VerticalTabStripFocusSwipeController::ResetGesture() {
  gesture_state_ = GestureState::kNone;
  cumulative_x_ = 0.0f;
  cumulative_y_ = 0.0f;
  has_triggered_in_current_gesture_ = false;
}

void VerticalTabStripFocusSwipeController::RotateTabGroupFocus(bool forward) {
  if (!region_view_ || !region_view_->tab_strip_model()) {
    return;
  }

  region_view_->tab_strip_model()->RotateFocusedGroup(forward);
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_gesture_handler.h"

#include "chromecast/base/chromecast_switches.h"

namespace chromecast {

namespace {
constexpr int kDefaultBackGestureHorizontalThreshold = 80;
}  // namespace

CastContentGestureHandler::CastContentGestureHandler(
    base::WeakPtr<CastContentWindow::Delegate> delegate,
    bool enable_top_drag_gesture)
    : priority_(Priority::NONE),
      enable_top_drag_gesture_(enable_top_drag_gesture),
      back_horizontal_threshold_(
          GetSwitchValueInt(switches::kBackGestureHorizontalThreshold,
                            kDefaultBackGestureHorizontalThreshold)),
      delegate_(delegate) {
  DCHECK(delegate_);
}

CastContentGestureHandler::CastContentGestureHandler(
    base::WeakPtr<CastContentWindow::Delegate> delegate)
    : CastContentGestureHandler(
          delegate,
          GetSwitchValueBoolean(switches::kEnableTopDragGesture, false)) {}

CastContentGestureHandler::~CastContentGestureHandler() = default;

void CastContentGestureHandler::SetPriority(
    CastGestureHandler::Priority priority) {
  priority_ = priority;
}

CastGestureHandler::Priority CastContentGestureHandler::GetPriority() {
  return priority_;
}

bool CastContentGestureHandler::CanHandleSwipe(
    CastSideSwipeOrigin swipe_origin) {
  if (!delegate_)
    return false;
  return delegate_->CanHandleGesture(GestureForSwipeOrigin(swipe_origin));
}

GestureType CastContentGestureHandler::GestureForSwipeOrigin(
    CastSideSwipeOrigin swipe_origin) {
  switch (swipe_origin) {
    case CastSideSwipeOrigin::LEFT:
      return GestureType::GO_BACK;
    case CastSideSwipeOrigin::TOP:
      return enable_top_drag_gesture_ ? GestureType::TOP_DRAG
                                      : GestureType::NO_GESTURE;
    case CastSideSwipeOrigin::RIGHT:
      return GestureType::RIGHT_DRAG;
    default:
      return GestureType::NO_GESTURE;
  }
}

void CastContentGestureHandler::HandleSideSwipe(
    CastSideSwipeEvent event,
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  if (!CanHandleSwipe(swipe_origin)) {
    return;
  }
  GestureType gesture_type = GestureForSwipeOrigin(swipe_origin);

  switch (event) {
    case CastSideSwipeEvent::BEGIN:
      current_swipe_time_ = base::ElapsedTimer();
      if (gesture_type == GestureType::GO_BACK) {
        DVLOG(1) << "back swipe gesture begin";
      }
      break;
    case CastSideSwipeEvent::CONTINUE:
      DVLOG(1) << "swipe gesture continue, elapsed time: "
               << current_swipe_time_.Elapsed().InMilliseconds() << "ms";
      delegate_->GestureProgress(gesture_type, touch_location);
      break;
    case CastSideSwipeEvent::END:
      DVLOG(1) << "swipe end, elapsed time: "
               << current_swipe_time_.Elapsed().InMilliseconds() << "ms";
      // If it's a back gesture, we have special handling to check for the
      // horizontal threshold. If the finger has lifted before the horizontal
      // gesture, cancel the back gesture and do not consume it.
      if (gesture_type == GestureType::GO_BACK &&
          touch_location.x() < back_horizontal_threshold_) {
        DVLOG(1) << "swipe gesture cancelled";
        delegate_->CancelGesture(GestureType::GO_BACK, touch_location);
        return;
      }
      delegate_->ConsumeGesture(gesture_type);
      DVLOG(1) << "gesture complete, elapsed time: "
               << current_swipe_time_.Elapsed().InMilliseconds() << "ms";
      break;
  }
}

void CastContentGestureHandler::HandleTapDownGesture(
    const gfx::Point& touch_location) {
  if (!delegate_ || !delegate_->CanHandleGesture(GestureType::TAP_DOWN)) {
    return;
  }
  delegate_->ConsumeGesture(GestureType::TAP_DOWN);
}

void CastContentGestureHandler::HandleTapGesture(
    const gfx::Point& touch_location) {
  if (!delegate_ || !delegate_->CanHandleGesture(GestureType::TAP)) {
    return;
  }
  delegate_->ConsumeGesture(GestureType::TAP);
}

}  // namespace chromecast

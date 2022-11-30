// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/platform_ui_input_delegate.h"

#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/vr/platform_input_handler.h"

namespace vr {

namespace {

static constexpr gfx::PointF kOutOfBoundsPoint = {-0.5f, -0.5f};

}  // namespace

PlatformUiInputDelegate::PlatformUiInputDelegate() {}
PlatformUiInputDelegate::PlatformUiInputDelegate(
    PlatformInputHandler* input_handler)
    : input_handler_(input_handler) {}

PlatformUiInputDelegate::~PlatformUiInputDelegate() = default;

void PlatformUiInputDelegate::OnHoverEnter(
    const gfx::PointF& normalized_hit_point,
    base::TimeTicks timestamp) {
  SendGestureToTarget(
      MakeInputEvent(InputEvent::kHoverEnter, normalized_hit_point, timestamp));
}

void PlatformUiInputDelegate::OnHoverLeave(base::TimeTicks timestamp) {
  // Note that we send an out of bounds hover leave event. With blink feature
  // UpdateHoverPostLayout turned on, a MouseMove event will dispatched post a
  // Layout. Sending a mouse leave event at 0,0 will result continuous
  // MouseMove events sent to the content if the content keeps relayout itself.
  // See https://crbug.com/762573 for details.
  SendGestureToTarget(
      MakeInputEvent(InputEvent::kHoverLeave, kOutOfBoundsPoint, timestamp));
}

void PlatformUiInputDelegate::OnHoverMove(
    const gfx::PointF& normalized_hit_point,
    base::TimeTicks timestamp) {
  SendGestureToTarget(
      MakeInputEvent(InputEvent::kHoverMove, normalized_hit_point, timestamp));
}

void PlatformUiInputDelegate::OnButtonDown(
    const gfx::PointF& normalized_hit_point,
    base::TimeTicks timestamp) {
  SendGestureToTarget(
      MakeInputEvent(InputEvent::kButtonDown, normalized_hit_point, timestamp));
}

void PlatformUiInputDelegate::OnButtonUp(
    const gfx::PointF& normalized_hit_point,
    base::TimeTicks timestamp) {
  SendGestureToTarget(
      MakeInputEvent(InputEvent::kButtonUp, normalized_hit_point, timestamp));
}

void PlatformUiInputDelegate::OnTouchMove(
    const gfx::PointF& normalized_hit_point,
    base::TimeTicks timestamp) {
  SendGestureToTarget(
      MakeInputEvent(InputEvent::kMove, normalized_hit_point, timestamp));
}

void PlatformUiInputDelegate::OnInputEvent(
    std::unique_ptr<InputEvent> event,
    const gfx::PointF& normalized_hit_point) {
  UpdateGesture(normalized_hit_point, event.get());
  SendGestureToTarget(std::move(event));
}

void PlatformUiInputDelegate::UpdateGesture(
    const gfx::PointF& normalized_content_hit_point,
    InputEvent* gesture) {
  gesture->set_position_in_widget(
      ScalePoint(normalized_content_hit_point, size_.width(), size_.height()));
}

void PlatformUiInputDelegate::SendGestureToTarget(
    std::unique_ptr<InputEvent> event) {
  if (!event || !input_handler_)
    return;

  input_handler_->ForwardEventToPlatformUi(std::move(event));
}

std::unique_ptr<InputEvent> PlatformUiInputDelegate::MakeInputEvent(
    InputEvent::Type type,
    const gfx::PointF& normalized_web_content_location,
    base::TimeTicks time_stamp) const {
  gfx::Point location = CalculateLocation(normalized_web_content_location);

  auto event = std::make_unique<InputEvent>(type);
  event->set_time_stamp(time_stamp);
  event->SetPositionInWidget(location.x(), location.y());
  return event;
}

gfx::Point PlatformUiInputDelegate::CalculateLocation(
    const gfx::PointF& normalized_web_content_location) const {
  return gfx::Point(size_.width() * normalized_web_content_location.x(),
                    size_.height() * normalized_web_content_location.y());
}

}  // namespace vr

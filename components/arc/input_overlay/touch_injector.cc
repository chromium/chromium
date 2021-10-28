// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/touch_injector.h"

#include "components/arc/input_overlay/actions/action.h"
#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace arc {

TouchInjector::TouchInjector(aura::Window* top_level_window)
    : target_window_(top_level_window) {}

TouchInjector::~TouchInjector() {
  UnRegisterEventRewriter();
}

void TouchInjector::ParseActions(const base::Value& root) {
  auto parsed_actions = ParseJsonToActions(target_window_, root);
  if (!parsed_actions)
    return;
  std::move(parsed_actions->begin(), parsed_actions->end(),
            std::back_inserter(actions_));
}

void TouchInjector::NotifyTextInputState(bool active) {
  if (text_input_active_ != active && active)
    DispatchTouchCancelEvent();
  text_input_active_ = active;
}

void TouchInjector::RegisterEventRewriter() {
  if (observation_.IsObserving())
    return;
  observation_.Observe(target_window_->GetHost()->GetEventSource());
}

void TouchInjector::UnRegisterEventRewriter() {
  if (!observation_.IsObserving())
    return;
  DispatchTouchCancelEvent();
  observation_.Reset();
}

void TouchInjector::DispatchTouchCancelEvent() {
  for (auto& action : actions_) {
    auto cancel = action->GetTouchCancelEvent();
    if (!cancel)
      continue;
    if (SendEventFinally(continuation_, &*cancel).dispatcher_destroyed) {
      VLOG(0) << "Undispatched event due to destroyed dispatcher for canceling "
                 "touch event.";
    }
    action->OnTouchCancelled();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventRewriter
ui::EventDispatchDetails TouchInjector::RewriteEvent(
    const ui::Event& event,
    const ui::EventRewriter::Continuation continuation) {
  continuation_ = continuation;
  if (text_input_active_)
    return SendEvent(continuation, &event);

  auto* widget = views::Widget::GetWidgetForNativeView(target_window_);
  DCHECK(widget->non_client_view());
  auto* frame_view = widget->non_client_view()->frame_view();
  DCHECK(frame_view);
  int height = frame_view->GetWindowBoundsForClientBounds(gfx::Rect()).y();
  auto bounds = gfx::RectF(target_window_->bounds());
  bounds.Offset(0, -height);

  std::list<ui::TouchEvent> touch_events;
  for (auto& action : actions_) {
    bool rewritten = action->RewriteEvent(event, touch_events, bounds);
    if (!rewritten)
      continue;
    if (touch_events.empty())
      return DiscardEvent(continuation);
    if (touch_events.size() == 1)
      return SendEventFinally(continuation, &touch_events.front());
    // TODO (cuicuiruan): Add handling for touch_events more than 1.
  }
  return SendEvent(continuation, &event);
}

}  // namespace arc

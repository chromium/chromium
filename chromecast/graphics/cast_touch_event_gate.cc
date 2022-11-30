// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_touch_event_gate.h"

#include "chromecast/graphics/cast_touch_activity_observer.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace chromecast {

CastTouchEventGate::CastTouchEventGate(aura::Window* root_window)
    : root_window_(root_window) {
  DCHECK(root_window);
  root_window_->GetHost()->GetEventSource()->AddEventRewriter(this);
}

CastTouchEventGate::~CastTouchEventGate() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
  if (enabled_) {
    for (auto* observer : observers_) {
      observer->OnTouchEventsDisabled(false);
    }
  }
}

void CastTouchEventGate::AddObserver(CastTouchActivityObserver* observer) {
  observers_.insert(observer);
}

void CastTouchEventGate::RemoveObserver(CastTouchActivityObserver* observer) {
  observers_.erase(observer);
}

void CastTouchEventGate::SetEnabled(bool enabled) {
  enabled_ = enabled;
  for (auto* observer : observers_) {
    observer->OnTouchEventsDisabled(enabled_);
  }
}

ui::EventDispatchDetails CastTouchEventGate::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!enabled_ || !event.IsTouchEvent())
    return SendEvent(continuation, &event);

  for (auto* observer : observers_) {
    observer->OnTouchActivity();
  }
  return DiscardEvent(continuation);
}

}  // namespace chromecast

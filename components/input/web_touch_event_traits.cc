// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/web_touch_event_traits.h"

#include <stddef.h>

#include "base/check.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace input {

bool WebTouchEventTraits::AllTouchPointsHaveState(
    const WebTouchEvent& event,
    blink::WebTouchPoint::State state) {
  if (!event.touches_length)
    return false;
  for (size_t i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].state != state)
      return false;
  }
  return true;
}

void WebTouchEventTraits::ResetType(WebInputEvent::Type type,
                                    base::TimeTicks timestamp,
                                    WebTouchEvent* event) {
  DCHECK(WebInputEvent::IsTouchEventType(type));
  DCHECK(type != WebInputEvent::Type::kTouchScrollStarted);

  event->SetType(type);
  event->dispatch_type = type == WebInputEvent::Type::kTouchCancel
                             ? WebInputEvent::DispatchType::kEventNonBlocking
                             : WebInputEvent::DispatchType::kBlocking;
  event->touch_start_or_first_touch_move &=
      (type == WebInputEvent::Type::kTouchStart ||
       type == WebInputEvent::Type::kTouchMove);
  event->SetTimeStamp(timestamp);
}

void WebTouchEventTraits::ResetTypeAndTouchStates(WebInputEvent::Type type,
                                                  base::TimeTicks timestamp,
                                                  WebTouchEvent* event) {
  ResetType(type, timestamp, event);

  WebTouchPoint::State newState = WebTouchPoint::State::kStateUndefined;
  switch (event->GetType()) {
    case WebInputEvent::Type::kTouchStart:
      newState = WebTouchPoint::State::kStatePressed;
      break;
    case WebInputEvent::Type::kTouchMove:
      newState = WebTouchPoint::State::kStateMoved;
      break;
    case WebInputEvent::Type::kTouchEnd:
      newState = WebTouchPoint::State::kStateReleased;
      break;
    case WebInputEvent::Type::kTouchCancel:
      newState = WebTouchPoint::State::kStateCancelled;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  for (size_t i = 0; i < event->touches_length; ++i)
    event->touches[i].state = newState;
}

}  // namespace input

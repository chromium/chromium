// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_WEB_TOUCH_EVENT_TRAITS_H_
#define COMPONENTS_INPUT_WEB_TOUCH_EVENT_TRAITS_H_

#include "base/time/time.h"
#include "base/component_export.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

namespace blink {
class WebTouchEvent;
}

namespace input {

// Utility class for performing operations on and with WebTouchEvents.
class COMPONENT_EXPORT(INPUT) WebTouchEventTraits {
 public:
  // Returns whether all touches in the event have the specified state.
  static bool AllTouchPointsHaveState(const blink::WebTouchEvent& event,
                                      blink::WebTouchPoint::State state);

  // Sets the type of |event| to |type|, resetting any other type-specific
  // properties and updating the timestamp.
  static void ResetType(blink::WebInputEvent::Type type,
                        base::TimeTicks timestamp,
                        blink::WebTouchEvent* event);

  // Like ResetType but also resets the state of all active touches
  // to match the event type.  This is particularly useful, for example,
  // in sending a touchcancel for all active touches.
  static void ResetTypeAndTouchStates(blink::WebInputEvent::Type type,
                                      base::TimeTicks timestamp,
                                      blink::WebTouchEvent* event);
};

}  // namespace input

#endif  // COMPONENTS_INPUT_WEB_TOUCH_EVENT_TRAITS_H_

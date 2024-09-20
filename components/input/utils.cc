// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/utils.h"

#include "build/build_config.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "components/input/features.h"
#endif

namespace input {

using blink::WebInputEvent;
using blink::mojom::InputEventResultState;
using perfetto::protos::pbzero::ChromeLatencyInfo2;

bool IsTransferInputToVizSupported() {
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(input::features::kInputOnViz) &&
      (base::android::BuildInfo::GetInstance()->sdk_int() >=
       base::android::SdkVersion::SDK_VERSION_V)) {
    return true;
  }
#endif
  return false;
}

ChromeLatencyInfo2::InputType InputEventTypeToProto(
    blink::WebInputEvent::Type event_type) {
  switch (event_type) {
    case WebInputEvent::Type::kUndefined:
      return ChromeLatencyInfo2::InputType::UNDEFINED_EVENT;
    case WebInputEvent::Type::kMouseDown:
      return ChromeLatencyInfo2::InputType::MOUSE_DOWN_EVENT;
    case WebInputEvent::Type::kMouseUp:
      return ChromeLatencyInfo2::InputType::MOUSE_UP_EVENT;
    case WebInputEvent::Type::kMouseMove:
      return ChromeLatencyInfo2::InputType::MOUSE_MOVE_EVENT;
    case WebInputEvent::Type::kMouseEnter:
      return ChromeLatencyInfo2::InputType::MOUSE_ENTER_EVENT;
    case WebInputEvent::Type::kMouseLeave:
      return ChromeLatencyInfo2::InputType::MOUSE_LEAVE_EVENT;
    case WebInputEvent::Type::kContextMenu:
      return ChromeLatencyInfo2::InputType::CONTEXT_MENU_EVENT;
    case WebInputEvent::Type::kMouseWheel:
      return ChromeLatencyInfo2::InputType::MOUSE_WHEEL_EVENT;
    case WebInputEvent::Type::kRawKeyDown:
      return ChromeLatencyInfo2::InputType::RAW_KEY_DOWN_EVENT;
    case WebInputEvent::Type::kKeyDown:
      return ChromeLatencyInfo2::InputType::KEY_DOWN_EVENT;
    case WebInputEvent::Type::kKeyUp:
      return ChromeLatencyInfo2::InputType::KEY_UP_EVENT;
    case WebInputEvent::Type::kChar:
      return ChromeLatencyInfo2::InputType::CHAR_EVENT;
    case WebInputEvent::Type::kGestureScrollBegin:
      return ChromeLatencyInfo2::InputType::GESTURE_SCROLL_BEGIN_EVENT;
    case WebInputEvent::Type::kGestureScrollEnd:
      return ChromeLatencyInfo2::InputType::GESTURE_SCROLL_END_EVENT;
    case WebInputEvent::Type::kGestureScrollUpdate:
      return ChromeLatencyInfo2::InputType::GESTURE_SCROLL_UPDATE_EVENT;
    case WebInputEvent::Type::kGestureFlingStart:
      return ChromeLatencyInfo2::InputType::GESTURE_FLING_START_EVENT;
    case WebInputEvent::Type::kGestureFlingCancel:
      return ChromeLatencyInfo2::InputType::GESTURE_FLING_CANCEL_EVENT;
    case WebInputEvent::Type::kGesturePinchBegin:
      return ChromeLatencyInfo2::InputType::GESTURE_PINCH_BEGIN_EVENT;
    case WebInputEvent::Type::kGesturePinchEnd:
      return ChromeLatencyInfo2::InputType::GESTURE_PINCH_END_EVENT;
    case WebInputEvent::Type::kGesturePinchUpdate:
      return ChromeLatencyInfo2::InputType::GESTURE_PINCH_UPDATE_EVENT;
    case WebInputEvent::Type::kGestureBegin:
      return ChromeLatencyInfo2::InputType::GESTURE_BEGIN_EVENT;
    case WebInputEvent::Type::kGestureTapDown:
      return ChromeLatencyInfo2::InputType::GESTURE_TAP_DOWN_EVENT;
    case WebInputEvent::Type::kGestureShowPress:
      return ChromeLatencyInfo2::InputType::GESTURE_SHOW_PRESS_EVENT;
    case WebInputEvent::Type::kGestureTap:
      return ChromeLatencyInfo2::InputType::GESTURE_TAP_EVENT;
    case WebInputEvent::Type::kGestureTapCancel:
      return ChromeLatencyInfo2::InputType::GESTURE_TAP_CANCEL_EVENT;
    case WebInputEvent::Type::kGestureShortPress:
      return ChromeLatencyInfo2::InputType::GESTURE_SHORT_PRESS_EVENT;
    case WebInputEvent::Type::kGestureLongPress:
      return ChromeLatencyInfo2::InputType::GESTURE_LONG_PRESS_EVENT;
    case WebInputEvent::Type::kGestureLongTap:
      return ChromeLatencyInfo2::InputType::GESTURE_LONG_TAP_EVENT;
    case WebInputEvent::Type::kGestureTwoFingerTap:
      return ChromeLatencyInfo2::InputType::GESTURE_TWO_FINGER_TAP_EVENT;
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      return ChromeLatencyInfo2::InputType::GESTURE_TAP_UNCONFIRMED_EVENT;
    case WebInputEvent::Type::kGestureDoubleTap:
      return ChromeLatencyInfo2::InputType::GESTURE_DOUBLE_TAP_EVENT;
    case WebInputEvent::Type::kGestureEnd:
      return ChromeLatencyInfo2::InputType::GESTURE_END_EVENT;
    case WebInputEvent::Type::kTouchStart:
      return ChromeLatencyInfo2::InputType::TOUCH_START_EVENT;
    case WebInputEvent::Type::kTouchMove:
      return ChromeLatencyInfo2::InputType::TOUCH_MOVE_EVENT;
    case WebInputEvent::Type::kTouchEnd:
      return ChromeLatencyInfo2::InputType::TOUCH_END_EVENT;
    case WebInputEvent::Type::kTouchCancel:
      return ChromeLatencyInfo2::InputType::TOUCH_CANCEL_EVENT;
    case WebInputEvent::Type::kTouchScrollStarted:
      return ChromeLatencyInfo2::InputType::TOUCH_SCROLL_STARTED_EVENT;
    case WebInputEvent::Type::kPointerDown:
      return ChromeLatencyInfo2::InputType::POINTER_DOWN_EVENT;
    case WebInputEvent::Type::kPointerUp:
      return ChromeLatencyInfo2::InputType::POINTER_UP_EVENT;
    case WebInputEvent::Type::kPointerMove:
      return ChromeLatencyInfo2::InputType::POINTER_MOVE_EVENT;
    case WebInputEvent::Type::kPointerRawUpdate:
      return ChromeLatencyInfo2::InputType::POINTER_RAW_UPDATE_EVENT;
    case WebInputEvent::Type::kPointerCancel:
      return ChromeLatencyInfo2::InputType::POINTER_CANCEL_EVENT;
    case WebInputEvent::Type::kPointerCausedUaAction:
      return ChromeLatencyInfo2::InputType::POINTER_CAUSED_UA_ACTION_EVENT;
  }
}

ChromeLatencyInfo2::InputResultState InputEventResultStateToProto(
    InputEventResultState result_state) {
  switch (result_state) {
    case InputEventResultState::kUnknown:
      return ChromeLatencyInfo2::InputResultState::UNKNOWN;
    case InputEventResultState::kConsumed:
      return ChromeLatencyInfo2::InputResultState::CONSUMED;
    case InputEventResultState::kNotConsumed:
      return ChromeLatencyInfo2::InputResultState::NOT_CONSUMED;
    case InputEventResultState::kNoConsumerExists:
      return ChromeLatencyInfo2::InputResultState::NO_CONSUMER_EXISTS;
    case InputEventResultState::kIgnored:
      return ChromeLatencyInfo2::InputResultState::IGNORED;
    case InputEventResultState::kSetNonBlocking:
      return ChromeLatencyInfo2::InputResultState::SET_NON_BLOCKING;
    case InputEventResultState::kSetNonBlockingDueToFling:
      return ChromeLatencyInfo2::InputResultState::
          SET_NON_BLOCKING_DUE_TO_FLING;
  }
}

}  // namespace input

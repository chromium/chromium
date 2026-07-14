// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_input_observer.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "components/cast_receiver/proto/input_event.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/types/scroll_types.h"

namespace cast_receiver {
namespace {

MouseEvent::ActionType MapActionType(blink::WebInputEvent::Type type) {
  switch (type) {
    case blink::WebInputEvent::Type::kMouseDown:
      return MouseEvent::MOUSE_DOWN;
    case blink::WebInputEvent::Type::kMouseUp:
      return MouseEvent::MOUSE_UP;
    case blink::WebInputEvent::Type::kMouseMove:
      return MouseEvent::MOUSE_MOVE;
    case blink::WebInputEvent::Type::kMouseEnter:
    case blink::WebInputEvent::Type::kMouseLeave:
    default:
      return MouseEvent::UNKNOWN;
  }
}

TouchEvent::ActionType MapTouchActionType(blink::WebInputEvent::Type type) {
  switch (type) {
    case blink::WebInputEvent::Type::kTouchStart:
      return TouchEvent::TOUCH_DOWN;
    case blink::WebInputEvent::Type::kTouchMove:
      return TouchEvent::TOUCH_MOVE;
    case blink::WebInputEvent::Type::kTouchEnd:
      return TouchEvent::TOUCH_UP;
    case blink::WebInputEvent::Type::kTouchCancel:
      return TouchEvent::TOUCH_CANCEL;
    default:
      return TouchEvent::UNKNOWN;
  }
}

void MapPressedButtons(int modifiers, MouseEvent* proto) {
  if (modifiers & blink::WebInputEvent::Modifiers::kLeftButtonDown) {
    proto->add_buttons(MouseEvent::LEFT_BUTTON);
  }
  if (modifiers & blink::WebInputEvent::Modifiers::kRightButtonDown) {
    proto->add_buttons(MouseEvent::RIGHT_BUTTON);
  }
  if (modifiers & blink::WebInputEvent::Modifiers::kMiddleButtonDown) {
    proto->add_buttons(MouseEvent::MIDDLE_BUTTON);
  }
  if (modifiers & blink::WebInputEvent::Modifiers::kBackButtonDown) {
    proto->add_buttons(MouseEvent::BROWSER_BACK_BUTTON);
  }
  if (modifiers & blink::WebInputEvent::Modifiers::kForwardButtonDown) {
    proto->add_buttons(MouseEvent::BROWSER_FORWARD_BUTTON);
  }
}

KeyboardEvent::ActionType MapKeyActionType(blink::WebInputEvent::Type type) {
  switch (type) {
    // Both rawKeyDown and keyDown map to KEY_DOWN. These events are mutually
    // exclusive for a single physical keystroke (Android sends KeyDown, while
    // Desktop/Fuchsia sends RawKeyDown + Char), so handling both does not
    // cause duplicate down events.
    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyDown:
      return KeyboardEvent::KEY_DOWN;
    case blink::WebInputEvent::Type::kKeyUp:
      return KeyboardEvent::KEY_UP;
    default:
      return KeyboardEvent::UNKNOWN;
  }
}

constexpr float kPixelsPerLineStep = 40.0f;

}  // namespace

StreamingInputObserver::StreamingInputObserver(
    content::WebContents* web_contents,
    InputEventCallback callback)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback)) {
  CHECK(web_contents);

  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  if (rfh) {
    content::RenderWidgetHost* rwh = rfh->GetRenderWidgetHost();
    if (rwh) {
      observed_widget_ = rwh;
      observed_widget_->AddObserver(this);
      observed_widget_->AddInputEventObserver(this);
    }
  }
}

StreamingInputObserver::~StreamingInputObserver() {
  if (observed_widget_) {
    observed_widget_->RemoveInputEventObserver(this);
    observed_widget_->RemoveObserver(this);
    observed_widget_ = nullptr;
  }
}

void StreamingInputObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (new_host && new_host == web_contents()->GetPrimaryMainFrame()) {
    if (observed_widget_) {
      observed_widget_->RemoveInputEventObserver(this);
      observed_widget_->RemoveObserver(this);
      observed_widget_ = nullptr;
    }
    content::RenderWidgetHost* new_rwh = new_host->GetRenderWidgetHost();
    if (new_rwh) {
      observed_widget_ = new_rwh;
      observed_widget_->AddObserver(this);
      observed_widget_->AddInputEventObserver(this);
    }
  }
}

void StreamingInputObserver::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  if (observed_widget_ == widget_host) {
    observed_widget_->RemoveInputEventObserver(this);
    observed_widget_->RemoveObserver(this);
    observed_widget_ = nullptr;
  }
}

void StreamingInputObserver::OnInputEvent(const content::RenderWidgetHost& host,
                                          const blink::WebInputEvent& event,
                                          InputEventSource source) {
  if (!web_contents()) {
    return;
  }

  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  if (!view) {
    return;
  }

  gfx::Size visible_viewport_size = view->GetVisibleViewportSize();
  if (visible_viewport_size.IsEmpty()) {
    return;
  }

  std::optional<cast_receiver::InputEvent> input_event_proto;

  if (event.GetType() == blink::WebInputEvent::Type::kMouseWheel) {
    std::optional<MouseEvent> mouse_proto = HandleMouseWheelEvent(
        static_cast<const blink::WebMouseWheelEvent&>(event),
        visible_viewport_size);
    if (mouse_proto) {
      cast_receiver::InputEvent wrapper;
      *wrapper.mutable_mouse_event() = std::move(*mouse_proto);
      input_event_proto = std::move(wrapper);
    }
  } else if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    std::optional<MouseEvent> mouse_proto = HandleMouseEvent(
        static_cast<const blink::WebMouseEvent&>(event), visible_viewport_size);
    if (mouse_proto) {
      cast_receiver::InputEvent wrapper;
      *wrapper.mutable_mouse_event() = std::move(*mouse_proto);
      input_event_proto = std::move(wrapper);
    }
  } else if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    std::optional<TouchEvent> touch_proto = HandleTouchEvent(
        static_cast<const blink::WebTouchEvent&>(event), visible_viewport_size);
    if (touch_proto) {
      cast_receiver::InputEvent wrapper;
      *wrapper.mutable_touch_event() = std::move(*touch_proto);
      input_event_proto = std::move(wrapper);
    }
  } else if (blink::WebInputEvent::IsKeyboardEventType(event.GetType())) {
    std::optional<KeyboardEvent> keyboard_proto =
        HandleKeyEvent(static_cast<const blink::WebKeyboardEvent&>(event));
    if (keyboard_proto) {
      cast_receiver::InputEvent wrapper;
      *wrapper.mutable_keyboard_event() = std::move(*keyboard_proto);
      input_event_proto = std::move(wrapper);
    }
  }

  if (input_event_proto) {
    input_event_proto->set_timestamp_ms(
        (event.TimeStamp() - base::TimeTicks()).InMilliseconds());
    callback_.Run(std::move(*input_event_proto));
  }
}

std::optional<cast_receiver::MouseEvent>
StreamingInputObserver::HandleMouseEvent(
    const blink::WebMouseEvent& mouse_event,
    const gfx::Size& visible_viewport_size) {
  MouseEvent proto;
  proto.set_action_type(MapActionType(mouse_event.GetType()));
  if (proto.action_type() == MouseEvent::UNKNOWN) {
    return std::nullopt;  // Ignore unknown mouse events
  }

  float x_ratio =
      mouse_event.PositionInWidget().x() / visible_viewport_size.width();
  float y_ratio =
      mouse_event.PositionInWidget().y() / visible_viewport_size.height();
  proto.set_x_ratio(std::clamp(x_ratio, 0.0f, 1.0f));
  proto.set_y_ratio(std::clamp(y_ratio, 0.0f, 1.0f));

  float move_x_ratio = static_cast<float>(mouse_event.movement_x) /
                       visible_viewport_size.width();
  float move_y_ratio = static_cast<float>(mouse_event.movement_y) /
                       visible_viewport_size.height();
  proto.set_move_x_ratio(move_x_ratio);
  proto.set_move_y_ratio(move_y_ratio);

  int modifiers = mouse_event.GetModifiers();
  proto.set_alt_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kAltKey));
  proto.set_ctrl_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kControlKey));
  proto.set_shift_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kShiftKey));
  proto.set_meta_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kMetaKey));

  MapPressedButtons(modifiers, &proto);

  return proto;
}

std::optional<cast_receiver::TouchEvent>
StreamingInputObserver::HandleTouchEvent(
    const blink::WebTouchEvent& touch_event,
    const gfx::Size& visible_viewport_size) {
  TouchEvent proto;
  proto.set_action_type(MapTouchActionType(touch_event.GetType()));
  if (proto.action_type() == TouchEvent::UNKNOWN) {
    return std::nullopt;
  }

  for (unsigned i = 0; i < touch_event.touches_length; ++i) {
    const blink::WebTouchPoint& point = touch_event.touches[i];
    // Filter for active touches still in contact with the surface.
    // The Exo TouchEvent proto does not have explicit release/up events.
    // Instead, the receiver tracks which touches have stopped by checking which
    // IDs are no longer present in the active list. Therefore, we exclude
    // kStateReleased and kStateCancelled touch points here.
    if (point.state == blink::mojom::TouchState::kStateReleased ||
        point.state == blink::mojom::TouchState::kStateCancelled) {
      continue;
    }

    Touch* touch_proto = proto.add_touches();
    touch_proto->set_id(point.id);

    float x_ratio =
        point.PositionInWidget().x() / visible_viewport_size.width();
    float y_ratio =
        point.PositionInWidget().y() / visible_viewport_size.height();
    touch_proto->set_x_ratio(std::clamp(x_ratio, 0.0f, 1.0f));
    touch_proto->set_y_ratio(std::clamp(y_ratio, 0.0f, 1.0f));
  }

  int modifiers = touch_event.GetModifiers();
  proto.set_alt_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kAltKey));
  proto.set_ctrl_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kControlKey));
  proto.set_shift_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kShiftKey));
  proto.set_meta_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kMetaKey));

  return proto;
}

std::optional<cast_receiver::MouseEvent>
StreamingInputObserver::HandleMouseWheelEvent(
    const blink::WebMouseWheelEvent& wheel_event,
    const gfx::Size& visible_viewport_size) {
  MouseEvent proto;
  proto.set_action_type(MouseEvent::MOUSE_WHEEL);

  float x_ratio =
      wheel_event.PositionInWidget().x() / visible_viewport_size.width();
  float y_ratio =
      wheel_event.PositionInWidget().y() / visible_viewport_size.height();
  proto.set_x_ratio(std::clamp(x_ratio, 0.0f, 1.0f));
  proto.set_y_ratio(std::clamp(y_ratio, 0.0f, 1.0f));

  // Resolve the scroll delta to pixels based on the granularity of the event,
  // before converting it to a viewport ratio.
  float delta_x = wheel_event.delta_x;
  float delta_y = wheel_event.delta_y;

  switch (wheel_event.delta_units) {
    case ui::ScrollGranularity::kScrollByPage:
    case ui::ScrollGranularity::kScrollByDocument:
      // Page scroll maps to the size of the viewport. Document scroll maps to
      // the entire document size, but we approximate it to viewport size as a
      // fallback.
      delta_x *= visible_viewport_size.width();
      delta_y *= visible_viewport_size.height();
      break;
    case ui::ScrollGranularity::kScrollByLine:
      // 1 line scroll maps to a default line step in pixels (40 DIPs).
      // Matches cc::kPixelsPerLineStep.
      delta_x *= kPixelsPerLineStep;
      delta_y *= kPixelsPerLineStep;
      break;
    case ui::ScrollGranularity::kScrollByPixel:
    case ui::ScrollGranularity::kScrollByPrecisePixel:
    default:
      break;
  }

  proto.set_move_x_ratio(delta_x / visible_viewport_size.width());
  proto.set_move_y_ratio(delta_y / visible_viewport_size.height());

  int modifiers = wheel_event.GetModifiers();
  proto.set_alt_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kAltKey));
  proto.set_ctrl_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kControlKey));
  proto.set_shift_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kShiftKey));
  proto.set_meta_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kMetaKey));

  MapPressedButtons(modifiers, &proto);

  return proto;
}

std::optional<cast_receiver::KeyboardEvent>
StreamingInputObserver::HandleKeyEvent(const blink::WebKeyboardEvent& event) {
  KeyboardEvent::ActionType action_type = MapKeyActionType(event.GetType());
  if (action_type == KeyboardEvent::UNKNOWN) {
    return std::nullopt;  // Ignore unknown keyboard events (like Char)
  }

  cast_receiver::KeyboardEvent proto;
  proto.set_action_type(action_type);

  std::string key_code = ui::KeycodeConverter::DomCodeToCodeString(
      static_cast<ui::DomCode>(event.dom_code));
  if (!key_code.empty()) {
    proto.set_key_code(key_code);
  }

  std::string key_value = ui::KeycodeConverter::DomKeyToKeyString(
      static_cast<ui::DomKey>(event.dom_key));
  if (!key_value.empty()) {
    proto.set_key_value(key_value);
  }

  int modifiers = event.GetModifiers();
  proto.set_repeat(
      !!(modifiers & blink::WebInputEvent::Modifiers::kIsAutoRepeat));
  proto.set_alt_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kAltKey));
  proto.set_ctrl_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kControlKey));
  proto.set_shift_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kShiftKey));
  proto.set_meta_key_press(
      !!(modifiers & blink::WebInputEvent::Modifiers::kMetaKey));
  proto.set_caps_lock_enabled(
      !!(modifiers & blink::WebInputEvent::Modifiers::kCapsLockOn));

  // The timestamp truncated to the whole millisecond.
  proto.set_timestamp_ms(
      (event.TimeStamp() - base::TimeTicks()).InMilliseconds());

  return proto;
}

}  // namespace cast_receiver

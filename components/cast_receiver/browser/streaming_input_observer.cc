// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_input_observer.h"

#include <algorithm>

#include "base/check.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

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

}  // namespace

StreamingInputObserver::StreamingInputObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
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

  if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    std::optional<MouseEvent> proto = HandleMouseEvent(
        static_cast<const blink::WebMouseEvent&>(event), visible_viewport_size);
    if (proto) {
      // TODO(b/501522425): Feed into OpenScreen input stream.
    }
  }

  // TODO(b/501521818): Implement translation for touch, keyboard, and wheel
  // events.
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

}  // namespace cast_receiver

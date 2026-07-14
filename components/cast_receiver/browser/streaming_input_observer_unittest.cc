// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_input_observer.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "components/cast_receiver/proto/keyboard_input_service.pb.h"
#include "components/cast_receiver/proto/touch_input_service.pb.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/types/scroll_types.h"

namespace cast_receiver {

class StreamingInputObserverTest : public content::RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(web_contents()->GetRenderWidgetHostView());
    web_contents()->GetRenderWidgetHostView()->SetSize(gfx::Size(1000, 500));
  }

  gfx::Size visible_viewport_size() const { return gfx::Size(1000, 500); }

  std::optional<MouseEvent> HandleMouseEvent(
      StreamingInputObserver& observer,
      const blink::WebMouseEvent& mouse_event,
      const gfx::Size& visible_viewport_size) {
    return observer.HandleMouseEvent(mouse_event, visible_viewport_size);
  }

  std::optional<TouchEvent> HandleTouchEvent(
      StreamingInputObserver& observer,
      const blink::WebTouchEvent& touch_event,
      const gfx::Size& visible_viewport_size) {
    return observer.HandleTouchEvent(touch_event, visible_viewport_size);
  }

  std::optional<MouseEvent> HandleMouseWheelEvent(
      StreamingInputObserver& observer,
      const blink::WebMouseWheelEvent& wheel_event,
      const gfx::Size& visible_viewport_size) {
    return observer.HandleMouseWheelEvent(wheel_event, visible_viewport_size);
  }

  std::optional<KeyboardEvent> HandleKeyEvent(
      StreamingInputObserver& observer,
      const blink::WebKeyboardEvent& key_event) {
    return observer.HandleKeyEvent(key_event);
  }
};

TEST_F(StreamingInputObserverTest, TranslateMouseMove) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(250.0f, 100.0f));
  mouse_event.movement_x = 10;
  mouse_event.movement_y = -5;

  std::optional<MouseEvent> opt_proto =
      HandleMouseEvent(observer, mouse_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const MouseEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), MouseEvent::MOUSE_MOVE);
  EXPECT_FLOAT_EQ(proto.x_ratio(), 0.25f);
  EXPECT_FLOAT_EQ(proto.y_ratio(), 0.20f);
  EXPECT_FLOAT_EQ(proto.move_x_ratio(), 0.01f);
  EXPECT_FLOAT_EQ(proto.move_y_ratio(), -0.01f);
  EXPECT_EQ(proto.buttons_size(), 0);
}

TEST_F(StreamingInputObserverTest, TranslateModifiers) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kControlKey | blink::WebInputEvent::kAltKey |
          blink::WebInputEvent::kShiftKey | blink::WebInputEvent::kMetaKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(250.0f, 100.0f));

  std::optional<MouseEvent> opt_proto =
      HandleMouseEvent(observer, mouse_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const MouseEvent& proto = opt_proto.value();
  EXPECT_TRUE(proto.ctrl_key_press());
  EXPECT_TRUE(proto.alt_key_press());
  EXPECT_TRUE(proto.shift_key_press());
  EXPECT_TRUE(proto.meta_key_press());
}

TEST_F(StreamingInputObserverTest, TranslateMouseDownLeftButton) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kLeftButtonDown | blink::WebInputEvent::kAltKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetPositionInWidget(gfx::PointF(500.0f, 250.0f));

  std::optional<MouseEvent> opt_proto =
      HandleMouseEvent(observer, mouse_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const MouseEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), MouseEvent::MOUSE_DOWN);
  EXPECT_FLOAT_EQ(proto.x_ratio(), 0.50f);
  EXPECT_FLOAT_EQ(proto.y_ratio(), 0.50f);
  EXPECT_TRUE(proto.alt_key_press());
  EXPECT_FALSE(proto.ctrl_key_press());
  ASSERT_EQ(proto.buttons_size(), 1);
  EXPECT_EQ(proto.buttons(0), MouseEvent::LEFT_BUTTON);
}

TEST_F(StreamingInputObserverTest, TranslateMouseUpMultipleButtons) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseUp,
      blink::WebInputEvent::kRightButtonDown |
          blink::WebInputEvent::kMiddleButtonDown |
          blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetPositionInWidget(gfx::PointF(750.0f, 375.0f));

  std::optional<MouseEvent> opt_proto =
      HandleMouseEvent(observer, mouse_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const MouseEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), MouseEvent::MOUSE_UP);
  EXPECT_FLOAT_EQ(proto.x_ratio(), 0.75f);
  EXPECT_FLOAT_EQ(proto.y_ratio(), 0.75f);
  EXPECT_TRUE(proto.shift_key_press());
  EXPECT_FALSE(proto.ctrl_key_press());
  ASSERT_EQ(proto.buttons_size(), 2);
  EXPECT_EQ(proto.buttons(0), MouseEvent::RIGHT_BUTTON);
  EXPECT_EQ(proto.buttons(1), MouseEvent::MIDDLE_BUTTON);
}

TEST_F(StreamingInputObserverTest, EnterAndLeaveMappedToUnknown) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseEnter,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(250.0f, 100.0f));

  std::optional<MouseEvent> opt_proto =
      HandleMouseEvent(observer, mouse_event, visible_viewport_size());
  EXPECT_FALSE(opt_proto.has_value());
}

TEST_F(StreamingInputObserverTest, ClampsCoordinatesToViewportRatios) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(1200.0f, -50.0f));

  std::optional<MouseEvent> opt_proto =
      HandleMouseEvent(observer, mouse_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const MouseEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), MouseEvent::MOUSE_MOVE);
  EXPECT_FLOAT_EQ(proto.x_ratio(), 1.0f);
  EXPECT_FLOAT_EQ(proto.y_ratio(), 0.0f);
}

TEST_F(StreamingInputObserverTest, TranslateTouchStartWithActiveFiltering) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchStart, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 2;

  touch_event.touches[0].id = 101;
  touch_event.touches[0].state = blink::mojom::TouchState::kStatePressed;
  touch_event.touches[0].SetPositionInWidget(100.0f, 50.0f);

  touch_event.touches[1].id = 102;
  touch_event.touches[1].state = blink::mojom::TouchState::kStateReleased;
  touch_event.touches[1].SetPositionInWidget(300.0f, 150.0f);

  std::optional<TouchEvent> opt_proto =
      HandleTouchEvent(observer, touch_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const TouchEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), TouchEvent::TOUCH_DOWN);
  EXPECT_TRUE(proto.shift_key_press());
  EXPECT_FALSE(proto.ctrl_key_press());

  // Only the pressed touch point should be mapped
  ASSERT_EQ(proto.touches_size(), 1);
  EXPECT_EQ(proto.touches(0).id(), 101);
  EXPECT_FLOAT_EQ(proto.touches(0).x_ratio(), 0.10f);
  EXPECT_FLOAT_EQ(proto.touches(0).y_ratio(), 0.10f);
}

TEST_F(StreamingInputObserverTest, TranslateTouchMove) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebTouchEvent touch_event(
      blink::WebInputEvent::Type::kTouchMove, blink::WebInputEvent::kMetaKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  touch_event.touches_length = 1;

  touch_event.touches[0].id = 202;
  touch_event.touches[0].state = blink::mojom::TouchState::kStateMoved;
  touch_event.touches[0].SetPositionInWidget(500.0f, 250.0f);

  std::optional<TouchEvent> opt_proto =
      HandleTouchEvent(observer, touch_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const TouchEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), TouchEvent::TOUCH_MOVE);
  EXPECT_TRUE(proto.meta_key_press());
  ASSERT_EQ(proto.touches_size(), 1);
  EXPECT_EQ(proto.touches(0).id(), 202);
  EXPECT_FLOAT_EQ(proto.touches(0).x_ratio(), 0.50f);
  EXPECT_FLOAT_EQ(proto.touches(0).y_ratio(), 0.50f);
}

TEST_F(StreamingInputObserverTest, TranslateMouseWheelScrollByPixel) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::Type::kMouseWheel, blink::WebInputEvent::kAltKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.SetPositionInWidget(gfx::PointF(250.0f, 100.0f));
  wheel_event.delta_x = 20.0f;
  wheel_event.delta_y = -10.0f;
  wheel_event.delta_units = ui::ScrollGranularity::kScrollByPixel;

  std::optional<MouseEvent> opt_proto =
      HandleMouseWheelEvent(observer, wheel_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const MouseEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), MouseEvent::MOUSE_WHEEL);
  EXPECT_FLOAT_EQ(proto.x_ratio(), 0.25f);
  EXPECT_FLOAT_EQ(proto.y_ratio(), 0.20f);
  // Scroll deltas mapped to movement ratios
  EXPECT_FLOAT_EQ(proto.move_x_ratio(), 0.02f);
  EXPECT_FLOAT_EQ(proto.move_y_ratio(), -0.02f);
  EXPECT_TRUE(proto.alt_key_press());
  EXPECT_FALSE(proto.ctrl_key_press());
}

TEST_F(StreamingInputObserverTest, TranslateMouseWheelScrollByPage) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.SetPositionInWidget(gfx::PointF(250.0f, 100.0f));
  wheel_event.delta_x = 2.0f;
  wheel_event.delta_y = -1.0f;
  wheel_event.delta_units = ui::ScrollGranularity::kScrollByPage;

  std::optional<MouseEvent> opt_proto =
      HandleMouseWheelEvent(observer, wheel_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const MouseEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), MouseEvent::MOUSE_WHEEL);
  EXPECT_FLOAT_EQ(proto.x_ratio(), 0.25f);
  EXPECT_FLOAT_EQ(proto.y_ratio(), 0.20f);
  // Page scroll deltas are mapped directly to ratios
  EXPECT_FLOAT_EQ(proto.move_x_ratio(), 2.0f);
  EXPECT_FLOAT_EQ(proto.move_y_ratio(), -1.0f);
}

TEST_F(StreamingInputObserverTest, TranslateMouseWheelScrollByLine) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.SetPositionInWidget(gfx::PointF(250.0f, 100.0f));
  wheel_event.delta_x = 2.0f;
  wheel_event.delta_y = -1.0f;
  wheel_event.delta_units = ui::ScrollGranularity::kScrollByLine;

  std::optional<MouseEvent> opt_proto =
      HandleMouseWheelEvent(observer, wheel_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const MouseEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), MouseEvent::MOUSE_WHEEL);
  EXPECT_FLOAT_EQ(proto.x_ratio(), 0.25f);
  EXPECT_FLOAT_EQ(proto.y_ratio(), 0.20f);
  // Line scroll deltas mapped to movement ratios (2 * 40 / 1000 = 0.08, -1 * 40
  // / 500 = -0.08)
  EXPECT_FLOAT_EQ(proto.move_x_ratio(), 0.08f);
  EXPECT_FLOAT_EQ(proto.move_y_ratio(), -0.08f);
}

TEST_F(StreamingInputObserverTest, TranslateMouseWheelScrollByDocument) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  wheel_event.SetPositionInWidget(gfx::PointF(250.0f, 100.0f));
  wheel_event.delta_x = 2.0f;
  wheel_event.delta_y = -1.0f;
  wheel_event.delta_units = ui::ScrollGranularity::kScrollByDocument;

  std::optional<MouseEvent> opt_proto =
      HandleMouseWheelEvent(observer, wheel_event, visible_viewport_size());
  ASSERT_TRUE(opt_proto.has_value());

  const MouseEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), MouseEvent::MOUSE_WHEEL);
  EXPECT_FLOAT_EQ(proto.x_ratio(), 0.25f);
  EXPECT_FLOAT_EQ(proto.y_ratio(), 0.20f);
  // Document scroll deltas are mapped directly to ratios (fallback behavior
  // matching page scroll)
  EXPECT_FLOAT_EQ(proto.move_x_ratio(), 2.0f);
  EXPECT_FLOAT_EQ(proto.move_y_ratio(), -1.0f);
}

TEST_F(StreamingInputObserverTest, TranslateKeyDown) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kRawKeyDown, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.dom_code = static_cast<int>(ui::DomCode::US_A);
  key_event.dom_key = ui::DomKey::FromCharacter('A');

  std::optional<KeyboardEvent> opt_proto = HandleKeyEvent(observer, key_event);
  ASSERT_TRUE(opt_proto.has_value());

  const KeyboardEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), KeyboardEvent::KEY_DOWN);
  EXPECT_EQ(proto.key_code(), "KeyA");
  EXPECT_EQ(proto.key_value(), "A");
  EXPECT_FALSE(proto.repeat());
  EXPECT_TRUE(proto.shift_key_press());
  EXPECT_FALSE(proto.ctrl_key_press());
}

TEST_F(StreamingInputObserverTest, TranslateKeyDownNonRaw) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kShiftKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.dom_code = static_cast<int>(ui::DomCode::US_A);
  key_event.dom_key = ui::DomKey::FromCharacter('A');

  std::optional<KeyboardEvent> opt_proto = HandleKeyEvent(observer, key_event);
  ASSERT_TRUE(opt_proto.has_value());

  const KeyboardEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), KeyboardEvent::KEY_DOWN);
  EXPECT_EQ(proto.key_code(), "KeyA");
  EXPECT_EQ(proto.key_value(), "A");
  EXPECT_FALSE(proto.repeat());
  EXPECT_TRUE(proto.shift_key_press());
  EXPECT_FALSE(proto.ctrl_key_press());
}

TEST_F(StreamingInputObserverTest, TranslateKeyUpWithRepeatAndCapsLock) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyUp,
      blink::WebInputEvent::Modifiers::kIsAutoRepeat |
          blink::WebInputEvent::Modifiers::kCapsLockOn,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_event.dom_code = static_cast<int>(ui::DomCode::TAB);
  key_event.dom_key = ui::DomKey::TAB;

  std::optional<KeyboardEvent> opt_proto = HandleKeyEvent(observer, key_event);
  ASSERT_TRUE(opt_proto.has_value());

  const KeyboardEvent& proto = opt_proto.value();
  EXPECT_EQ(proto.action_type(), KeyboardEvent::KEY_UP);
  EXPECT_EQ(proto.key_code(), "Tab");
  EXPECT_EQ(proto.key_value(), "Tab");
  EXPECT_TRUE(proto.repeat());
  EXPECT_TRUE(proto.caps_lock_enabled());
  EXPECT_FALSE(proto.shift_key_press());
}

TEST_F(StreamingInputObserverTest, IgnoreCharEvents) {
  StreamingInputObserver observer(web_contents(), base::DoNothing());

  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kChar, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  std::optional<KeyboardEvent> opt_proto = HandleKeyEvent(observer, key_event);
  EXPECT_FALSE(opt_proto.has_value());
}

TEST_F(StreamingInputObserverTest, CallbackTriggeredOnMouseEvent) {
  std::optional<cast_receiver::InputEvent> received_event;
  auto callback = base::BindRepeating(
      [](std::optional<cast_receiver::InputEvent>* dest,
         const cast_receiver::InputEvent& event) { *dest = event; },
      base::Unretained(&received_event));

  StreamingInputObserver observer(web_contents(), std::move(callback));

  content::RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(250.0f, 100.0f));
  mouse_event.movement_x = 10;
  mouse_event.movement_y = -5;

  rwh->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(received_event.has_value());
  EXPECT_TRUE(received_event->has_mouse_event());
  const MouseEvent& proto = received_event->mouse_event();
  EXPECT_EQ(proto.action_type(), MouseEvent::MOUSE_MOVE);
  EXPECT_FLOAT_EQ(proto.x_ratio(), 0.25f);
  EXPECT_FLOAT_EQ(proto.y_ratio(), 0.20f);
}

}  // namespace cast_receiver

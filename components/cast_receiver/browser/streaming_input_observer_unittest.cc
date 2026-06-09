// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_input_observer.h"

#include <optional>

#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

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
};

TEST_F(StreamingInputObserverTest, TranslateMouseMove) {
  StreamingInputObserver observer(web_contents());

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
  StreamingInputObserver observer(web_contents());

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
  StreamingInputObserver observer(web_contents());

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
  StreamingInputObserver observer(web_contents());

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
  StreamingInputObserver observer(web_contents());

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
  StreamingInputObserver observer(web_contents());

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

}  // namespace cast_receiver

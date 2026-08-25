// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/stylus_handwriting_handler_win.h"

#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "components/input/input_router_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "ui/gfx/geometry/size.h"

namespace input {
namespace {

using blink::SyntheticWebGestureEventBuilder;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using GestureHandlingResult = StylusHandwritingHandler::GestureHandlingResult;

class TestStylusInterface final : public StylusInterface {
 public:
  bool ShouldInitiateStylusWriting() override { return true; }
  void NotifyHoverActionStylusWritable(bool) override {}
};

class TestInputRouterClient final : public InputRouterClient {
 public:
  blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent&,
      const ui::LatencyInfo&) override {
    return blink::mojom::InputEventResultState::kNotConsumed;
  }
  void IncrementInFlightEventCount() override {}
  void DecrementInFlightEventCount(
      blink::mojom::InputEventResultSource) override {}
  void DidOverscroll(blink::mojom::DidOverscrollParamsPtr) override {}
  void OnSetCompositorAllowedTouchAction(cc::TouchAction) override {}
  void DidStartScrollingViewport() override {}
  void OnInputRouterActive() override {}
  void ForwardGestureEventWithLatencyInfo(const blink::WebGestureEvent&,
                                          const ui::LatencyInfo&) override {}
  void ForwardWheelEventWithLatencyInfo(const blink::WebMouseWheelEvent&,
                                        const ui::LatencyInfo&) override {}
  bool IsWheelScrollInProgress() override { return false; }
  bool IsAutoscrollInProgress() override { return false; }
  void SetMouseCapture(bool) override {}
  void SetAutoscrollSelectionActiveInMainFrame(bool) override {}
  void RequestMouseLock(
      bool,
      bool,
      blink::mojom::WidgetInputHandlerHost::RequestMouseLockCallback) override {
  }
  gfx::Size GetRootWidgetViewportSize() override { return gfx::Size(); }
  void OnInvalidInputEventSource() override {}
  blink::mojom::WidgetInputHandler* GetWidgetInputHandler() override {
    return nullptr;
  }
  void OnImeCancelComposition() override {}
  void OnImeCompositionRangeChanged(
      const gfx::Range&,
      const std::optional<std::vector<gfx::Rect>>&) override {}
  StylusInterface* GetStylusInterface() override { return &stylus_interface_; }
  void OnStartStylusWriting() override { ++start_stylus_writing_count_; }
  void OnUnconfirmedTapConvertedToTap() override {}
  DispatchToRendererCallback GetDispatchToRendererCallback() override {
    return base::DoNothing();
  }

  int start_stylus_writing_count() const { return start_stylus_writing_count_; }

 private:
  TestStylusInterface stylus_interface_;
  int start_stylus_writing_count_ = 0;
};

cc::TouchAction WritableTouchAction() {
  return cc::TouchAction::kAuto & ~cc::TouchAction::kInternalNotWritable;
}

}  // namespace

class StylusHandwritingHandlerWinTest : public testing::Test {
 protected:
  uint32_t SendPenTouchStart() { return next_touch_event_id_++; }

  WebGestureEvent BuildGesture(
      WebInputEvent::Type type,
      uint32_t touch_start_event_id,
      ui::EventPointerType pointer_type = ui::EventPointerType::kPen,
      uint32_t source_touch_event_id = 0) {
    WebGestureEvent gesture = SyntheticWebGestureEventBuilder::Build(
        type, WebGestureDevice::kTouchscreen);
    gesture.primary_pointer_type = pointer_type;
    gesture.primary_unique_touch_event_id = touch_start_event_id;
    gesture.unique_touch_event_id =
        source_touch_event_id ? source_touch_event_id : touch_start_event_id;
    return gesture;
  }

  void SendTapDown(uint32_t touch_start_event_id,
                   std::optional<cc::TouchAction> allowed_touch_action =
                       WritableTouchAction()) {
    EXPECT_EQ(GestureHandlingResult::kNotHandled,
              handler_.HandleGesture(
                  BuildGesture(WebInputEvent::Type::kGestureTapDown,
                               touch_start_event_id),
                  allowed_touch_action));
  }

  uint32_t StartHandwritingSequence(
      std::optional<cc::TouchAction> allowed_touch_action =
          WritableTouchAction()) {
    const uint32_t touch_start_event_id = SendPenTouchStart();
    SendTapDown(touch_start_event_id, allowed_touch_action);
    return touch_start_event_id;
  }

  GestureHandlingResult SendShowPress(uint32_t touch_start_event_id,
                                      uint32_t source_touch_event_id = 0) {
    return handler_.HandleGesture(
        BuildGesture(WebInputEvent::Type::kGestureShowPress,
                     touch_start_event_id, ui::EventPointerType::kPen,
                     source_touch_event_id),
        WritableTouchAction());
  }

  TestInputRouterClient client_;
  StylusHandwritingHandlerWin handler_{&client_};
  uint32_t next_touch_event_id_ = 1;
};

TEST_F(StylusHandwritingHandlerWinTest, TapDownCreatesHandwritingSequence) {
  const uint32_t touch_start_event_id = SendPenTouchStart();
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());

  SendTapDown(touch_start_event_id);

  EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());
}

TEST_F(StylusHandwritingHandlerWinTest,
       TapAfterTimerShowPressWritesWithoutMovement) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));

  EXPECT_EQ(
      GestureHandlingResult::kForwardAsTapCancel,
      handler_.HandleGesture(
          BuildGesture(WebInputEvent::Type::kGestureTap, touch_start_event_id),
          WritableTouchAction()));

  EXPECT_EQ(1, client_.start_stylus_writing_count());
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
}

TEST_F(StylusHandwritingHandlerWinTest,
       TapAfterSynthesizedShowPressDoesNotWrite) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();

  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id, touch_start_event_id + 1));
  EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());
  EXPECT_EQ(
      GestureHandlingResult::kNotHandled,
      handler_.HandleGesture(
          BuildGesture(WebInputEvent::Type::kGestureTap, touch_start_event_id),
          WritableTouchAction()));
  EXPECT_EQ(0, client_.start_stylus_writing_count());
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
}

TEST_F(StylusHandwritingHandlerWinTest, TapWithoutShowPressIsUnhandled) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();

  EXPECT_EQ(
      GestureHandlingResult::kNotHandled,
      handler_.HandleGesture(
          BuildGesture(WebInputEvent::Type::kGestureTap, touch_start_event_id),
          WritableTouchAction()));
  EXPECT_EQ(0, client_.start_stylus_writing_count());
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
}

TEST_F(StylusHandwritingHandlerWinTest,
       TouchAndEraserDoNotCreateHandwritingSequences) {
  EXPECT_EQ(
      GestureHandlingResult::kNotHandled,
      handler_.HandleGesture(BuildGesture(WebInputEvent::Type::kGestureTapDown,
                                          /*touch_start_event_id=*/1,
                                          ui::EventPointerType::kTouch),
                             WritableTouchAction()));
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());

  EXPECT_EQ(
      GestureHandlingResult::kNotHandled,
      handler_.HandleGesture(BuildGesture(WebInputEvent::Type::kGestureTapDown,
                                          /*touch_start_event_id=*/2,
                                          ui::EventPointerType::kEraser),
                             WritableTouchAction()));
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
}

}  // namespace input

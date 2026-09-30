// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/stylus_handwriting_handler_win.h"

#include <cmath>
#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "components/input/input_router_client.h"
#include "components/stylus_handwriting/win/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace input {
namespace {

using blink::SyntheticWebGestureEventBuilder;
using blink::SyntheticWebTouchEvent;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using GestureHandlingResult = StylusHandwritingHandler::GestureHandlingResult;

constexpr float kTestDeviceScaleFactor = 2.f;
constexpr float kTestPixelsPerInchX = 192.f;
constexpr float kTestPixelsPerInchY = 288.f;
constexpr float kTestHimetricPerInch = 2540.f;
constexpr float kTestHimetricPerDipX =
    kTestDeviceScaleFactor * kTestHimetricPerInch / kTestPixelsPerInchX;
constexpr float kTestHimetricPerDipY =
    kTestDeviceScaleFactor * kTestHimetricPerInch / kTestPixelsPerInchY;
constexpr gfx::PointF kTouchStartPosition(1.f, 1.f);
constexpr gfx::Vector2dF kGestureScreenOffset(6.5f, 235.f);

class TestStylusInterface final : public StylusInterface {
 public:
  bool ShouldInitiateStylusWriting() override { return true; }
  void NotifyHoverActionStylusWritable(bool) override {}
  std::optional<gfx::Vector2dF> GetStylusHandwritingPixelsPerInch() override {
    return pixels_per_inch_;
  }

  void set_pixels_per_inch(std::optional<gfx::Vector2dF> pixels_per_inch) {
    pixels_per_inch_ = pixels_per_inch;
  }

 private:
  std::optional<gfx::Vector2dF> pixels_per_inch_ =
      gfx::Vector2dF(kTestPixelsPerInchX, kTestPixelsPerInchY);
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

  void set_stylus_pixels_per_inch(
      std::optional<gfx::Vector2dF> pixels_per_inch) {
    stylus_interface_.set_pixels_per_inch(pixels_per_inch);
  }

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
  float small_gesture_movement_threshold_in_himetric() const {
    return static_cast<float>(
        stylus_handwriting::win::
            StylusHandwritingWinSmallGestureMovementThresholdHm());
  }

  gfx::Vector2dF HimetricToDips(float x, float y) const {
    return gfx::Vector2dF(x / kTestHimetricPerDipX, y / kTestHimetricPerDipY);
  }

  uint32_t SendPenTouchStart() {
    const int point = touch_event_.PressPoint(kTouchStartPosition.x(),
                                              kTouchStartPosition.y());
    touch_event_.touches[point].pointer_type = ui::EventPointerType::kPen;
    handler_.OnTouchEvent(touch_event_, kTestDeviceScaleFactor);
    const uint32_t touch_start_event_id = touch_event_.unique_touch_event_id;
    touch_event_.ResetPoints();
    return touch_start_event_id;
  }

  uint32_t ReleaseTouchPoint(int point) {
    touch_event_.ReleasePoint(point);
    const uint32_t release_event_id = touch_event_.unique_touch_event_id;
    handler_.OnTouchEvent(touch_event_, kTestDeviceScaleFactor);
    touch_event_.ResetPoints();
    return release_event_id;
  }

  void MovePen(float delta_x, float delta_y) {
    touch_event_.MovePoint(0, kTouchStartPosition.x() + delta_x,
                           kTouchStartPosition.y() + delta_y);
    handler_.OnTouchEvent(touch_event_, kTestDeviceScaleFactor);
    touch_event_.ResetPoints();
  }

  void MovePenByHimetric(float delta_x, float delta_y) {
    const gfx::Vector2dF movement_in_dips = HimetricToDips(delta_x, delta_y);
    MovePen(movement_in_dips.x(), movement_in_dips.y());
  }

  WebGestureEvent BuildGesture(
      WebInputEvent::Type type,
      uint32_t touch_start_event_id,
      ui::EventPointerType pointer_type = ui::EventPointerType::kPen,
      uint32_t source_touch_event_id = 0) {
    WebGestureEvent gesture = SyntheticWebGestureEventBuilder::Build(
        type, WebGestureDevice::kTouchscreen);
    gesture.SetPositionInWidget(kTouchStartPosition);
    gesture.SetPositionInScreen(kTouchStartPosition + kGestureScreenOffset);
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
  SyntheticWebTouchEvent touch_event_;
};

TEST_F(StylusHandwritingHandlerWinTest, TouchStartCreatesHandwritingSequence) {
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());

  SendPenTouchStart();

  EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());
}

TEST_F(StylusHandwritingHandlerWinTest, PixelsPerInchIsLatchedAtTouchStart) {
  const uint32_t touch_start_event_id = SendPenTouchStart();
  client_.set_stylus_pixels_per_inch(
      gfx::Vector2dF(kTestPixelsPerInchX * 2.f, kTestPixelsPerInchY * 2.f));
  SendTapDown(touch_start_event_id);
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));

  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);

  EXPECT_EQ(1, client_.start_stylus_writing_count());
}

TEST_F(StylusHandwritingHandlerWinTest,
       TouchStartWithoutHandwritingPropertiesDoesNotCreateSequence) {
  client_.set_stylus_pixels_per_inch(std::nullopt);
  SendPenTouchStart();

  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
}

TEST_F(StylusHandwritingHandlerWinTest,
       VerticalMovementBelowThresholdDoesNotStartWriting) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));

  MovePenByHimetric(0.f, small_gesture_movement_threshold_in_himetric() - 1.f);

  EXPECT_EQ(0, client_.start_stylus_writing_count());
}

TEST_F(StylusHandwritingHandlerWinTest,
       ExceededMovementThresholdStartsUponShowPress) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();

  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  ASSERT_EQ(0, client_.start_stylus_writing_count());

  EXPECT_EQ(GestureHandlingResult::kConsumed,
            SendShowPress(touch_start_event_id));
  EXPECT_EQ(1, client_.start_stylus_writing_count());
}

TEST_F(StylusHandwritingHandlerWinTest,
       ShowPressStartsWhenMovementThresholdIsCrossed) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));

  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);

  EXPECT_EQ(1, client_.start_stylus_writing_count());
}

TEST_F(StylusHandwritingHandlerWinTest, DiagonalMovementUsesEuclideanDistance) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));

  // Each axis remains below the threshold while the Euclidean distance is
  // 1.01 times the threshold.
  const float movement_per_axis_in_himetric =
      small_gesture_movement_threshold_in_himetric() * 1.01f / std::sqrt(2.f);
  MovePenByHimetric(movement_per_axis_in_himetric,
                    movement_per_axis_in_himetric);

  EXPECT_EQ(1, client_.start_stylus_writing_count());
}

TEST_F(StylusHandwritingHandlerWinTest,
       TapAfterTimerShowPressWritesWithoutMovement) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));

  const uint32_t release_event_id = ReleaseTouchPoint(0);
  EXPECT_EQ(
      GestureHandlingResult::kForwardAsTapCancel,
      handler_.HandleGesture(
          BuildGesture(WebInputEvent::Type::kGestureTap, touch_start_event_id,
                       ui::EventPointerType::kPen, release_event_id),
          WritableTouchAction()));

  EXPECT_EQ(1, client_.start_stylus_writing_count());
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());

  const uint32_t next_touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(next_touch_start_event_id));
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  EXPECT_EQ(2, client_.start_stylus_writing_count());
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

TEST_F(StylusHandwritingHandlerWinTest, NonWritableTouchActionPreventsWriting) {
  const uint32_t touch_start_event_id =
      StartHandwritingSequence(cc::TouchAction::kAuto);
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);

  EXPECT_EQ(0, client_.start_stylus_writing_count());
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

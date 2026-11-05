// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/stylus_handwriting_handler_win.h"

#include <array>
#include <cmath>
#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
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

constexpr WebInputEvent::Type kTerminalTapGestureTypes[] = {
    WebInputEvent::Type::kGestureTap,
    WebInputEvent::Type::kGestureDoubleTap,
    WebInputEvent::Type::kGestureTwoFingerTap,
    WebInputEvent::Type::kGestureLongTap,
};

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

  uint32_t SendTouchStart(ui::EventPointerType pointer_type) {
    const int point = touch_event_.PressPoint(kTouchStartPosition.x(),
                                              kTouchStartPosition.y());
    touch_event_.touches[point].pointer_type = pointer_type;
    handler_.OnTouchEvent(touch_event_, kTestDeviceScaleFactor);
    const uint32_t touch_start_event_id = touch_event_.unique_touch_event_id;
    touch_event_.ResetPoints();
    return touch_start_event_id;
  }

  uint32_t SendPenTouchStart() {
    return SendTouchStart(ui::EventPointerType::kPen);
  }

  int AddTouchPoint(float x, float y) {
    const int point = touch_event_.PressPoint(x, y);
    handler_.OnTouchEvent(touch_event_, kTestDeviceScaleFactor);
    touch_event_.ResetPoints();
    return point;
  }

  uint32_t ReleaseTouchPoint(int point) {
    touch_event_.ReleasePoint(point);
    const uint32_t release_event_id = touch_event_.unique_touch_event_id;
    handler_.OnTouchEvent(touch_event_, kTestDeviceScaleFactor);
    touch_event_.ResetPoints();
    return release_event_id;
  }

  void MoveTouchPoint(int point, float x, float y) {
    touch_event_.MovePoint(point, x, y);
    handler_.OnTouchEvent(touch_event_, kTestDeviceScaleFactor);
    touch_event_.ResetPoints();
  }

  void MovePen(float delta_x, float delta_y) {
    MoveTouchPoint(0, kTouchStartPosition.x() + delta_x,
                   kTouchStartPosition.y() + delta_y);
  }

  void MovePenByHimetric(float delta_x, float delta_y) {
    const gfx::Vector2dF movement_in_dips = HimetricToDips(delta_x, delta_y);
    MovePen(movement_in_dips.x(), movement_in_dips.y());
  }

  void PressAdditionalTouch() {
    const int point = touch_event_.PressPoint(2.f, 2.f);
    touch_event_.touches[point].pointer_type = ui::EventPointerType::kTouch;
    handler_.OnTouchEvent(touch_event_, kTestDeviceScaleFactor);
    touch_event_.ResetPoints();
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
  EXPECT_FALSE(handler_.StartStylusWriting());
}

TEST_F(StylusHandwritingHandlerWinTest,
       AdditionalContactDisqualifiesUnstartedSequence) {
  for (bool late_touch_action : {false, true}) {
    SCOPED_TRACE(late_touch_action);
    const uint32_t id = StartHandwritingSequence(
        late_touch_action ? std::nullopt
                          : std::make_optional(WritableTouchAction()));
    SendShowPress(id);
    if (late_touch_action) {
      MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f,
                        0.f);
    }
    ASSERT_FALSE(handler_.StylusWritingStarted());
    PressAdditionalTouch();
    EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());
    EXPECT_FALSE(handler_.StartStylusWriting());
    handler_.ApplyTouchAction(WritableTouchAction());
    MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f,
                      0.f);
    EXPECT_EQ(0, client_.start_stylus_writing_count());
    ReleaseTouchPoint(1);
    const uint32_t release_id = ReleaseTouchPoint(0);
    handler_.HandleGesture(BuildGesture(WebInputEvent::Type::kGestureEnd, id,
                                        ui::EventPointerType::kPen, release_id),
                           WritableTouchAction());
    EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
  }
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

  EXPECT_EQ(GestureHandlingResult::kForwardAsTapCancel,
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

TEST_F(StylusHandwritingHandlerWinTest,
       TerminalTapGesturesClearUnstartedSequence) {
  for (WebInputEvent::Type type : kTerminalTapGestureTypes) {
    SCOPED_TRACE(WebInputEvent::GetName(type));
    const uint32_t touch_start_event_id = StartHandwritingSequence();
    ASSERT_TRUE(handler_.has_handwriting_sequence_for_testing());
    const uint32_t release_event_id = ReleaseTouchPoint(0);

    EXPECT_EQ(GestureHandlingResult::kNotHandled,
              handler_.HandleGesture(
                  BuildGesture(type, touch_start_event_id,
                               ui::EventPointerType::kPen, release_event_id),
                  WritableTouchAction()));
    EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
    EXPECT_FALSE(handler_.StylusWritingStarted());
  }
}

TEST_F(StylusHandwritingHandlerWinTest,
       TerminalTapGesturesEndStartedHandwriting) {
  for (WebInputEvent::Type type : kTerminalTapGestureTypes) {
    SCOPED_TRACE(WebInputEvent::GetName(type));
    const uint32_t touch_start_event_id = StartHandwritingSequence();
    EXPECT_EQ(GestureHandlingResult::kNotHandled,
              SendShowPress(touch_start_event_id));
    MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f,
                      0.f);
    ASSERT_TRUE(handler_.StylusWritingStarted());
    const uint32_t release_event_id = ReleaseTouchPoint(0);

    const GestureHandlingResult expected_result =
        type == WebInputEvent::Type::kGestureTap
            ? GestureHandlingResult::kForwardAsTapCancel
            : GestureHandlingResult::kConsumed;
    EXPECT_EQ(expected_result,
              handler_.HandleGesture(
                  BuildGesture(type, touch_start_event_id,
                               ui::EventPointerType::kPen, release_event_id),
                  WritableTouchAction()));
    EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
    EXPECT_FALSE(handler_.StylusWritingStarted());
  }
}

TEST_F(StylusHandwritingHandlerWinTest,
       EndStylusWritingClearsHandwritingSequence) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  ASSERT_TRUE(handler_.has_handwriting_sequence_for_testing());
  ASSERT_TRUE(handler_.StylusWritingStarted());

  handler_.EndStylusWriting();

  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
  EXPECT_FALSE(handler_.StylusWritingStarted());
}

TEST_F(StylusHandwritingHandlerWinTest, ShortPressPreventsScrollWriting) {
  for (bool late_touch_action : {false, true}) {
    SCOPED_TRACE(late_touch_action);
    const uint32_t touch_start_event_id = StartHandwritingSequence(
        late_touch_action ? std::nullopt
                          : std::make_optional(WritableTouchAction()));
    EXPECT_EQ(GestureHandlingResult::kNotHandled,
              SendShowPress(touch_start_event_id));
    MovePenByHimetric(0.f, small_gesture_movement_threshold_in_himetric() +
                               (late_touch_action ? 1.f : -1.f));
    ASSERT_FALSE(handler_.StylusWritingStarted());

    EXPECT_EQ(GestureHandlingResult::kNotHandled,
              handler_.HandleGesture(
                  BuildGesture(WebInputEvent::Type::kGestureShortPress,
                               touch_start_event_id),
                  WritableTouchAction()));
    EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());
    EXPECT_EQ(GestureHandlingResult::kNotHandled,
              handler_.HandleGesture(
                  BuildGesture(WebInputEvent::Type::kGestureLongPress,
                               touch_start_event_id),
                  WritableTouchAction()));
    handler_.ApplyTouchAction(WritableTouchAction());
    MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f,
                      0.f);
    EXPECT_FALSE(handler_.StylusWritingStarted());
    handler_.HandleGesture(
        BuildGesture(WebInputEvent::Type::kGestureScrollBegin,
                     touch_start_event_id),
        WritableTouchAction());
    EXPECT_FALSE(handler_.StartStylusWriting());
    EXPECT_EQ(0, client_.start_stylus_writing_count());
    const uint32_t release_id = ReleaseTouchPoint(0);
    handler_.HandleGesture(
        BuildGesture(WebInputEvent::Type::kGestureEnd, touch_start_event_id,
                     ui::EventPointerType::kPen, release_id),
        WritableTouchAction());
    EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
  }

  // Disqualification applies only to the front sequence.
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  const uint32_t release_id = ReleaseTouchPoint(0);
  const uint32_t next_touch_start_event_id = SendPenTouchStart();
  handler_.HandleGesture(BuildGesture(WebInputEvent::Type::kGestureShortPress,
                                      touch_start_event_id),
                         WritableTouchAction());
  EXPECT_EQ(2u, handler_.handwriting_sequence_count_for_testing());
  handler_.HandleGesture(
      BuildGesture(WebInputEvent::Type::kGestureEnd, touch_start_event_id,
                   ui::EventPointerType::kPen, release_id),
      WritableTouchAction());
  SendTapDown(next_touch_start_event_id);
  SendShowPress(next_touch_start_event_id);
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  EXPECT_EQ(1, client_.start_stylus_writing_count());
}

TEST_F(StylusHandwritingHandlerWinTest,
       StartedHandwritingPressGesturesAreHandled) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  ASSERT_EQ(1, client_.start_stylus_writing_count());

  EXPECT_EQ(GestureHandlingResult::kForwardAsTapCancel,
            handler_.HandleGesture(
                BuildGesture(WebInputEvent::Type::kGestureShortPress,
                             touch_start_event_id),
                WritableTouchAction()));
  EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());
  EXPECT_EQ(GestureHandlingResult::kConsumed,
            handler_.HandleGesture(
                BuildGesture(WebInputEvent::Type::kGestureLongPress,
                             touch_start_event_id),
                WritableTouchAction()));
  EXPECT_EQ(GestureHandlingResult::kConsumed,
            handler_.HandleGesture(
                BuildGesture(WebInputEvent::Type::kGestureTapCancel,
                             touch_start_event_id),
                WritableTouchAction()));
}

TEST_F(StylusHandwritingHandlerWinTest, NonWritableTouchActionPreventsWriting) {
  const uint32_t touch_start_event_id =
      StartHandwritingSequence(cc::TouchAction::kAuto);
  EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);

  EXPECT_EQ(0, client_.start_stylus_writing_count());

  // Disqualification applies only to the front sequence.
  const uint32_t release_id = ReleaseTouchPoint(0);
  const uint32_t next_touch_start_event_id = SendPenTouchStart();
  handler_.HandleGesture(
      BuildGesture(WebInputEvent::Type::kGestureEnd, touch_start_event_id,
                   ui::EventPointerType::kPen, release_id),
      WritableTouchAction());
  SendTapDown(next_touch_start_event_id);
  SendShowPress(next_touch_start_event_id);
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  EXPECT_EQ(1, client_.start_stylus_writing_count());
}

TEST_F(StylusHandwritingHandlerWinTest,
       TouchActionAndAdditionalContactPreserveStartedWriting) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  ASSERT_EQ(1, client_.start_stylus_writing_count());

  PressAdditionalTouch();
  ASSERT_TRUE(handler_.has_handwriting_sequence_for_testing());
  EXPECT_TRUE(handler_.StylusWritingStarted());
  handler_.ApplyTouchAction(cc::TouchAction::kAuto);
  EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());
  EXPECT_TRUE(handler_.StylusWritingStarted());
  EXPECT_EQ(GestureHandlingResult::kConsumed,
            handler_.HandleGesture(
                BuildGesture(WebInputEvent::Type::kGestureTwoFingerTap,
                             touch_start_event_id),
                WritableTouchAction()));
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
}

// GestureBegin identifies each contact's down event, not the sequence start.
// A second contact must not be mistaken for a newer handwriting sequence.
TEST_F(StylusHandwritingHandlerWinTest, AdditionalPenBeginPreservesWriting) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  ASSERT_TRUE(handler_.StylusWritingStarted());

  const uint32_t additional_contact = SendPenTouchStart();
  EXPECT_EQ(
      GestureHandlingResult::kNotHandled,
      handler_.HandleGesture(
          BuildGesture(WebInputEvent::Type::kGestureBegin, additional_contact),
          WritableTouchAction()));
  EXPECT_EQ(1u, handler_.handwriting_sequence_count_for_testing());
  EXPECT_TRUE(handler_.StylusWritingStarted());
  EXPECT_EQ(1, client_.start_stylus_writing_count());
}

TEST_F(StylusHandwritingHandlerWinTest,
       SecondaryGestureEndDoesNotClearHandwritingSequence) {
  const uint32_t touch_start_event_id = StartHandwritingSequence();
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            SendShowPress(touch_start_event_id));
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  ASSERT_EQ(1, client_.start_stylus_writing_count());

  const int secondary_point = AddTouchPoint(2.f, 2.f);
  const uint32_t secondary_release_event_id =
      ReleaseTouchPoint(secondary_point);
  EXPECT_EQ(
      GestureHandlingResult::kNotHandled,
      handler_.HandleGesture(
          BuildGesture(WebInputEvent::Type::kGestureEnd, touch_start_event_id,
                       ui::EventPointerType::kPen, secondary_release_event_id),
          WritableTouchAction()));
  EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());

  const uint32_t primary_release_event_id = ReleaseTouchPoint(0);
  EXPECT_EQ(
      GestureHandlingResult::kNotHandled,
      handler_.HandleGesture(
          BuildGesture(WebInputEvent::Type::kGestureEnd, touch_start_event_id,
                       ui::EventPointerType::kPen, primary_release_event_id),
          WritableTouchAction()));
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
}

TEST_F(StylusHandwritingHandlerWinTest,
       NonPenTouchStartPreservesPendingSequence) {
  for (ui::EventPointerType pointer_type :
       {ui::EventPointerType::kTouch, ui::EventPointerType::kEraser}) {
    SCOPED_TRACE(static_cast<int>(pointer_type));
    const uint32_t pen_id = SendPenTouchStart();
    const uint32_t release_id = ReleaseTouchPoint(0);
    ASSERT_TRUE(handler_.has_handwriting_sequence_for_testing());

    const uint32_t id = SendTouchStart(pointer_type);

    EXPECT_EQ(1u, handler_.handwriting_sequence_count_for_testing());
    EXPECT_EQ(GestureHandlingResult::kNotHandled,
              handler_.HandleGesture(
                  BuildGesture(WebInputEvent::Type::kGestureTapDown, id,
                               pointer_type),
                  WritableTouchAction()));
    EXPECT_EQ(1u, handler_.handwriting_sequence_count_for_testing());
    ReleaseTouchPoint(0);
    handler_.HandleGesture(
        BuildGesture(WebInputEvent::Type::kGestureEnd, pen_id,
                     ui::EventPointerType::kPen, release_id),
        WritableTouchAction());
    EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
  }
}

TEST_F(StylusHandwritingHandlerWinTest, TapDownMustMatchPenSequence) {
  const uint32_t previous_touch_start_event_id = SendPenTouchStart();
  ReleaseTouchPoint(0);
  handler_.HandleGesture(BuildGesture(WebInputEvent::Type::kGestureTap,
                                      previous_touch_start_event_id),
                         WritableTouchAction());
  const uint32_t touch_start_event_id = SendPenTouchStart();
  ASSERT_TRUE(handler_.has_handwriting_sequence_for_testing());

  EXPECT_EQ(
      GestureHandlingResult::kNotHandled,
      handler_.HandleGesture(BuildGesture(WebInputEvent::Type::kGestureTapDown,
                                          previous_touch_start_event_id),
                             WritableTouchAction()));

  EXPECT_TRUE(handler_.has_handwriting_sequence_for_testing());
  SendShowPress(touch_start_event_id);
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  EXPECT_EQ(0, client_.start_stylus_writing_count());
  SendTapDown(touch_start_event_id);
  EXPECT_EQ(1, client_.start_stylus_writing_count());
}

TEST_F(StylusHandwritingHandlerWinTest, QueuedSequencesStartInGestureOrder) {
  std::array<uint32_t, 3> starts;
  std::array<uint32_t, 3> releases;
  for (size_t i = 0; i < starts.size(); ++i) {
    starts[i] = SendPenTouchStart();
    releases[i] = ReleaseTouchPoint(0);
  }
  ASSERT_EQ(starts.size(), handler_.handwriting_sequence_count_for_testing());

  for (size_t i = 0; i < starts.size(); ++i) {
    SendTapDown(starts[i]);
    SendShowPress(starts[i]);
    EXPECT_EQ(GestureHandlingResult::kForwardAsTapCancel,
              handler_.HandleGesture(
                  BuildGesture(WebInputEvent::Type::kGestureTap, starts[i],
                               ui::EventPointerType::kPen, releases[i]),
                  WritableTouchAction()));
    EXPECT_EQ(static_cast<int>(i + 1), client_.start_stylus_writing_count());
    EXPECT_EQ(starts.size() - i - 1,
              handler_.handwriting_sequence_count_for_testing());
    // The following GestureEnd must not retire the next queued candidate.
    EXPECT_EQ(GestureHandlingResult::kNotHandled,
              handler_.HandleGesture(
                  BuildGesture(WebInputEvent::Type::kGestureEnd, starts[i],
                               ui::EventPointerType::kPen, releases[i]),
                  WritableTouchAction()));
    EXPECT_EQ(starts.size() - i - 1,
              handler_.handwriting_sequence_count_for_testing());
  }
}

// Asserts that when the handler receives a gesture newer than the first queued
// sequence, it clears any sequences with ids older than that of the gesture.
// This scenario shouldn't be possible while kEnableGestureBeginEndTypes is
// enabled. While it is enabled, sequences enqueued by unacked TouchStart
// are guaranteed to receive a terminating GestureEnd even if the TouchStart
// was consumed by the renderer.
TEST_F(StylusHandwritingHandlerWinTest, GesturesNewerThanQueuedSequences) {
  // Queue sequences for starts 0, 1, 2, 3, 5, and 6. Show that gestures with
  // ids matching starts 2, 4, and 7 clear older sequences as expected.
  std::array<uint32_t, 8> starts;
  for (size_t i = 0; i < starts.size(); ++i) {
    if (i == 4 || i == 7) {
      client_.set_stylus_pixels_per_inch(std::nullopt);
    } else {
      client_.set_stylus_pixels_per_inch(
          gfx::Vector2dF(kTestPixelsPerInchX, kTestPixelsPerInchY));
    }
    starts[i] = SendPenTouchStart();
    if (i == 0) {
      SendTapDown(starts[i]);
      SendShowPress(starts[i]);
      MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f,
                        0.f);
      ASSERT_TRUE(handler_.StylusWritingStarted());
    }
    ReleaseTouchPoint(0);
  }
  ASSERT_EQ(6u, handler_.handwriting_sequence_count_for_testing());

  const auto gesture_for_start_2 =
      BuildGesture(WebInputEvent::Type::kGestureTapDown, starts[2]);
  const auto gesture_for_start_4 =
      BuildGesture(WebInputEvent::Type::kGestureTapDown, starts[4]);
  const auto gesture_for_start_7 =
      BuildGesture(WebInputEvent::Type::kGestureTapDown, starts[7]);
#if DCHECK_IS_ON()
  EXPECT_DCHECK_DEATH(
      handler_.HandleGesture(gesture_for_start_2, WritableTouchAction()));
  EXPECT_DCHECK_DEATH(
      handler_.HandleGesture(gesture_for_start_4, WritableTouchAction()));
  EXPECT_DCHECK_DEATH(
      handler_.HandleGesture(gesture_for_start_7, WritableTouchAction()));
#else
  // A match for start 2 retires starts 0 and 1, leaving 2, 3, 5, and 6.
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            handler_.HandleGesture(gesture_for_start_2, WritableTouchAction()));
  EXPECT_EQ(4u, handler_.handwriting_sequence_count_for_testing());
  EXPECT_FALSE(handler_.StylusWritingStarted());
  EXPECT_EQ(1, client_.start_stylus_writing_count());

  // The unqueued start 4 retires starts 2 and 3, preserving 5 and 6.
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            handler_.HandleGesture(gesture_for_start_4, WritableTouchAction()));
  EXPECT_EQ(2u, handler_.handwriting_sequence_count_for_testing());

  // Start 7 is newer than every remaining entry, so the queue empties.
  EXPECT_EQ(GestureHandlingResult::kNotHandled,
            handler_.HandleGesture(gesture_for_start_7, WritableTouchAction()));
  EXPECT_FALSE(handler_.has_handwriting_sequence_for_testing());
#endif
}

TEST_F(StylusHandwritingHandlerWinTest,
       GestureEndRetiresSequenceWithoutTapDown) {
  // A consumed TouchStart drops the sequence's TapDown and subsequent gestures,
  // but GestureBegin/GestureEnd are still forwarded. The primary GestureEnd
  // must retire the sequence so it does not block the next one.
  const uint32_t first = SendPenTouchStart();
  const uint32_t release = ReleaseTouchPoint(0);
  const uint32_t second = SendPenTouchStart();
  handler_.HandleGesture(BuildGesture(WebInputEvent::Type::kGestureEnd, first,
                                      ui::EventPointerType::kPen, release),
                         WritableTouchAction());
  EXPECT_EQ(1u, handler_.handwriting_sequence_count_for_testing());
  SendTapDown(second);
  SendShowPress(second);
  MovePenByHimetric(small_gesture_movement_threshold_in_himetric() + 1.f, 0.f);
  EXPECT_EQ(1, client_.start_stylus_writing_count());
}

}  // namespace input

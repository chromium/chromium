// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/touch_action_filter.h"

#include "components/input/event_with_latency_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/events/blink/blink_features.h"

using blink::SyntheticWebGestureEventBuilder;
using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace input {
namespace {

const blink::WebGestureDevice kSourceDevice =
    blink::WebGestureDevice::kTouchscreen;

}  // namespace

class TouchActionFilterTest : public testing::Test {
 public:
  TouchActionFilterTest() { filter_.OnHasTouchEventHandlers(true); }
  ~TouchActionFilterTest() override = default;

 protected:
  std::optional<cc::TouchAction> ActiveTouchAction() const {
    return filter_.active_touch_action_;
  }
  void ResetTouchAction() { filter_.ResetTouchAction(); }
  void ResetActiveTouchAction() { filter_.active_touch_action_.reset(); }
  void ResetCompositorAllowedTouchAction() {
    filter_.compositor_allowed_touch_action_ = cc::TouchAction::kAuto;
  }
  void SetNoDeferredEvents() { filter_.has_deferred_events_ = false; }
  void SetGestureSequenceInProgress() {
    filter_.gesture_sequence_in_progress_ = true;
  }
  void ResetGestureSequenceInProgress() {
    filter_.gesture_sequence_in_progress_ = false;
  }
  void PanTest(cc::TouchAction action,
               float scroll_x,
               float scroll_y,
               float dx,
               float dy,
               float expected_dx,
               float expected_dy) {
    WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::Type::kGestureTapDown, kSourceDevice);
    WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

    {
      // Scrolls with no direction hint are permitted in the |action| direction.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();

      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(0, 0,
                                                            kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kAllowed);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                             kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(expected_dx, scroll_update.data.scroll_update.delta_x);
      EXPECT_EQ(expected_dy, scroll_update.data.scroll_update.delta_y);

      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kAllowed);
      filter_.DecreaseActiveTouches();
    }

    {
      // Scrolls biased towards the touch-action axis are permitted.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(scroll_x, scroll_y,
                                                            kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kAllowed);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                             kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(expected_dx, scroll_update.data.scroll_update.delta_x);
      EXPECT_EQ(expected_dy, scroll_update.data.scroll_update.delta_y);

      // Ensure that scrolls in the opposite direction are not filtered once
      // scrolling has started. (Once scrolling is started, the direction may
      // be reversed by the user even if scrolls that start in the reversed
      // direction are disallowed.
      WebGestureEvent scroll_update2 =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(-dx, -dy, 0,
                                                             kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update2),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(-expected_dx, scroll_update2.data.scroll_update.delta_x);
      EXPECT_EQ(-expected_dy, scroll_update2.data.scroll_update.delta_y);

      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kAllowed);
      filter_.DecreaseActiveTouches();
    }

    {
      // Scrolls biased towards the perpendicular of the touch-action axis are
      // suppressed entirely.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(scroll_y, scroll_x,
                                                            kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kFiltered);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                             kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kFiltered);
      EXPECT_EQ(dx, scroll_update.data.scroll_update.delta_x);
      EXPECT_EQ(dy, scroll_update.data.scroll_update.delta_y);

      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kFiltered);
      filter_.DecreaseActiveTouches();
    }
  }

  void PanTestForUnidirectionalTouchAction(cc::TouchAction action,
                                           float scroll_x,
                                           float scroll_y) {
    WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::Type::kGestureTapDown, kSourceDevice);
    WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

    {
      // Scrolls towards the touch-action direction are permitted.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(scroll_x, scroll_y,
                                                            kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kAllowed);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(scroll_x, scroll_y,
                                                             0, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kAllowed);
      filter_.DecreaseActiveTouches();
    }

    {
      // Scrolls towards the exact opposite of the touch-action direction are
      // suppressed entirely.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(
              -scroll_x, -scroll_y, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kFiltered);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(
              -scroll_x, -scroll_y, 0, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kFiltered);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kFiltered);
      filter_.DecreaseActiveTouches();
    }

    {
      // Scrolls towards the diagonal opposite of the touch-action direction are
      // suppressed entirely.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(
              -scroll_x - scroll_y, -scroll_x - scroll_y, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kFiltered);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(
              -scroll_x - scroll_y, -scroll_x - scroll_y, 0, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kFiltered);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kFiltered);
      filter_.DecreaseActiveTouches();
    }
  }
  TouchActionFilter filter_;
};

TEST_F(TouchActionFilterTest, SimpleFilter) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  const float kDeltaX = 5;
  const float kDeltaY = 10;
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDeltaX, kDeltaY, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

  // cc::TouchAction::kAuto doesn't cause any filtering.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(kDeltaX, scroll_update.data.scroll_update.delta_x);
  EXPECT_EQ(kDeltaY, scroll_update.data.scroll_update.delta_y);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);
  filter_.DecreaseActiveTouches();

  // cc::TouchAction::kNone filters out all scroll events, but no other events.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(kDeltaX, scroll_update.data.scroll_update.delta_x);
  EXPECT_EQ(kDeltaY, scroll_update.data.scroll_update.delta_y);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFiltered);
  filter_.DecreaseActiveTouches();

  // When a new touch sequence begins, the state is reset.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);
  filter_.DecreaseActiveTouches();

  // Setting touch action doesn't impact any in-progress gestures.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);
  filter_.DecreaseActiveTouches();

  // And the state is still cleared for the next gesture.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);
  filter_.DecreaseActiveTouches();

  // Changing the touch action during a gesture has no effect.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFiltered);
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(kDeltaX, scroll_update.data.scroll_update.delta_x);
  EXPECT_EQ(kDeltaY, scroll_update.data.scroll_update.delta_y);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFiltered);
  filter_.DecreaseActiveTouches();

  // horizontal scroll
  scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(3, 2, kSourceDevice);

  // kInternalPanXScrolls has no effect when active touch action is available.
  {
    ResetTouchAction();
    // With kInternalPanXScrolls
    filter_.OnSetTouchAction(cc::TouchAction::kPanX |
                             cc::TouchAction::kInternalPanXScrolls);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();

    ResetTouchAction();
    // Without kInternalPanXScrolls
    filter_.OnSetTouchAction(cc::TouchAction::kPanX);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();

    ResetTouchAction();
    // We only set kInternalPanXScrolls when kPanX is set, so there is no
    // kInternalPanXScrolls with kPanY case.
    filter_.OnSetTouchAction(cc::TouchAction::kPanY);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFiltered);
    filter_.DecreaseActiveTouches();
  }
}

TEST_F(TouchActionFilterTest, PanLeft) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 7;
  const float kScrollY = 6;

  PanTest(cc::TouchAction::kPanLeft, kScrollX, kScrollY, kDX, kDY, kDX, 0);
  PanTestForUnidirectionalTouchAction(cc::TouchAction::kPanLeft, kScrollX, 0);
}

TEST_F(TouchActionFilterTest, PanRight) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = -7;
  const float kScrollY = 6;

  PanTest(cc::TouchAction::kPanRight, kScrollX, kScrollY, kDX, kDY, kDX, 0);
  PanTestForUnidirectionalTouchAction(cc::TouchAction::kPanRight, kScrollX, 0);
}

TEST_F(TouchActionFilterTest, PanX) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 7;
  const float kScrollY = 6;

  PanTest(cc::TouchAction::kPanX, kScrollX, kScrollY, kDX, kDY, kDX, 0);
}

TEST_F(TouchActionFilterTest, PanUp) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 6;
  const float kScrollY = 7;

  PanTest(cc::TouchAction::kPanUp, kScrollX, kScrollY, kDX, kDY, 0, kDY);
  PanTestForUnidirectionalTouchAction(cc::TouchAction::kPanUp, 0, kScrollY);
}

TEST_F(TouchActionFilterTest, PanDown) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 6;
  const float kScrollY = -7;

  PanTest(cc::TouchAction::kPanDown, kScrollX, kScrollY, kDX, kDY, 0, kDY);
  PanTestForUnidirectionalTouchAction(cc::TouchAction::kPanDown, 0, kScrollY);
}

TEST_F(TouchActionFilterTest, PanY) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 6;
  const float kScrollY = 7;

  PanTest(cc::TouchAction::kPanY, kScrollX, kScrollY, kDX, kDY, 0, kDY);
}

TEST_F(TouchActionFilterTest, PanXY) {
  const float kDX = 5;
  const float kDY = 10;
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

  {
    // Scrolls hinted in the X axis are permitted and unmodified.
    ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kPan);
    filter_.IncreaseActiveTouches();
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-7, 6, kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kAllowed);

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDX, kDY, 0,
                                                           kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(kDX, scroll_update.data.scroll_update.delta_x);
    EXPECT_EQ(kDY, scroll_update.data.scroll_update.delta_y);

    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();
  }

  {
    // Scrolls hinted in the Y axis are permitted and unmodified.
    ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kPan);
    filter_.IncreaseActiveTouches();
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-6, 7, kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kAllowed);

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDX, kDY, 0,
                                                           kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(kDX, scroll_update.data.scroll_update.delta_x);
    EXPECT_EQ(kDY, scroll_update.data.scroll_update.delta_y);

    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();
  }

  {
    // A two-finger gesture is not allowed.
    ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kPan);
    filter_.IncreaseActiveTouches();
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-6, 7, kSourceDevice,
                                                          2);
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFiltered);

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDX, kDY, 0,
                                                           kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kFiltered);

    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFiltered);
    filter_.DecreaseActiveTouches();
  }
}

TEST_F(TouchActionFilterTest, BitMath) {
  // Verify that the simple flag mixing properties we depend on are now
  // trivially true.
  EXPECT_EQ(cc::TouchAction::kNone,
            cc::TouchAction::kNone & cc::TouchAction::kAuto);
  EXPECT_EQ(cc::TouchAction::kNone,
            cc::TouchAction::kPanY & cc::TouchAction::kPanX);
  EXPECT_EQ(cc::TouchAction::kPan,
            cc::TouchAction::kAuto & cc::TouchAction::kPan);
  EXPECT_EQ(cc::TouchAction::kManipulation,
            cc::TouchAction::kAuto & ~(cc::TouchAction::kDoubleTapZoom |
                                       cc::TouchAction::kInternalPanXScrolls |
                                       cc::TouchAction::kInternalNotWritable));
  EXPECT_EQ(cc::TouchAction::kPanX,
            cc::TouchAction::kPanLeft | cc::TouchAction::kPanRight);
  EXPECT_EQ(cc::TouchAction::kAuto, cc::TouchAction::kManipulation |
                                        cc::TouchAction::kDoubleTapZoom |
                                        cc::TouchAction::kInternalPanXScrolls |
                                        cc::TouchAction::kInternalNotWritable);
}

TEST_F(TouchActionFilterTest, MultiTouch) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  const float kDeltaX = 5;
  const float kDeltaY = 10;
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDeltaX, kDeltaY, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

  // For multiple points, the intersection is what matters.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(kDeltaX, scroll_update.data.scroll_update.delta_x);
  EXPECT_EQ(kDeltaY, scroll_update.data.scroll_update.delta_y);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFiltered);
  filter_.DecreaseActiveTouches();

  // Intersection of PAN_X and PAN_Y is NONE.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kPanX);
  filter_.OnSetTouchAction(cc::TouchAction::kPanY);
  filter_.OnSetTouchAction(cc::TouchAction::kPan);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFiltered);
  filter_.DecreaseActiveTouches();
}

class TouchActionFilterPinchTest : public testing::Test {
 public:
  TouchActionFilterPinchTest() = default;

  void RunTest(bool force_enable_zoom) {
    filter_.OnHasTouchEventHandlers(true);
    filter_.SetForceEnableZoom(force_enable_zoom);

    WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::Type::kGestureTapDown, kSourceDevice);
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice,
                                                          2);
    WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::Type::kGesturePinchBegin, kSourceDevice);
    WebGestureEvent pinch_update =
        SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                          kSourceDevice);
    WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::Type::kGesturePinchEnd, kSourceDevice);
    WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

    // Pinch is allowed with touch-action: auto.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kAuto);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();

    // Pinch is not allowed with touch-action: none.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kNone);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFiltered);
    filter_.DecreaseActiveTouches();

    // Pinch is not allowed with touch-action: pan-x pan-y except for force
    // enable zoom.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kPan);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&scroll_begin),
              force_enable_zoom ? FilterGestureEventResult::kFiltered
                                : FilterGestureEventResult::kAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&pinch_begin),
              force_enable_zoom ? FilterGestureEventResult::kFiltered
                                : FilterGestureEventResult::kAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&pinch_update),
              force_enable_zoom ? FilterGestureEventResult::kFiltered
                                : FilterGestureEventResult::kAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&pinch_end),
              force_enable_zoom ? FilterGestureEventResult::kFiltered
                                : FilterGestureEventResult::kAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&scroll_end),
              force_enable_zoom ? FilterGestureEventResult::kFiltered
                                : FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();

    // Pinch is allowed with touch-action: manipulation.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kManipulation);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kAllowed);
    EXPECT_FALSE(filter_.drop_pinch_events_);
    // The pinch gesture is always re-evaluated on pinch begin.
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();

    // Pinch state is automatically reset at the end of a scroll.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kAuto);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();

    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kAuto);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();

    // Scrolling is allowed when two fingers are down.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kPinchZoom);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kAllowed);
    filter_.DecreaseActiveTouches();

    // At double-tap-drag-zoom case, the pointer_count is 1 at GesturePinchBegin
    // and we need to evaluate whether the gesture is allowed or not at that
    // time.
    scroll_begin.data.scroll_begin.pointer_count = 1;
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::TouchAction::kPinchZoom);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFiltered);
    filter_.DecreaseActiveTouches();
  }

 private:
  TouchActionFilter filter_;
};

TEST_F(TouchActionFilterPinchTest, Pinch) {
  RunTest(false);
}

// Enables force enable zoom will override touch-action except for
// touch-action: none.
TEST_F(TouchActionFilterPinchTest, ForceEnableZoom) {
  RunTest(true);
}

TEST_F(TouchActionFilterTest, DoubleTapWithTouchActionAuto) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapCancel, kSourceDevice);
  WebGestureEvent double_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureDoubleTap, kSourceDevice);

  // Double tap is allowed with touch action auto.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&unconfirmed_tap),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(unconfirmed_tap.GetType(),
            WebInputEvent::Type::kGestureTapUnconfirmed);
  // The tap cancel will come as part of the next touch sequence.
  ResetTouchAction();
  // Changing the touch action for the second tap doesn't effect the behaviour
  // of the event.
  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&double_tap),
            FilterGestureEventResult::kAllowed);
}

TEST_F(TouchActionFilterTest, DoubleTap) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapCancel, kSourceDevice);
  WebGestureEvent double_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureDoubleTap, kSourceDevice);

  // Double tap is disabled with any touch action other than auto.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kManipulation);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&unconfirmed_tap),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(WebInputEvent::Type::kGestureTap, unconfirmed_tap.GetType());
  // Changing the touch action for the second tap doesn't effect the behaviour
  // of the event. The tap cancel will come as part of the next touch sequence.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&double_tap),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(WebInputEvent::Type::kGestureTap, double_tap.GetType());
  EXPECT_EQ(2, double_tap.data.tap.tap_count);
  filter_.DecreaseActiveTouches();
}

TEST_F(TouchActionFilterTest, SingleTapWithTouchActionAuto) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap1 = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTap, kSourceDevice);

  // Single tap is allowed with touch action auto.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&unconfirmed_tap1),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(WebInputEvent::Type::kGestureTapUnconfirmed,
            unconfirmed_tap1.GetType());
  EXPECT_EQ(filter_.FilterGestureEvent(&tap),
            FilterGestureEventResult::kAllowed);
}

TEST_F(TouchActionFilterTest, SingleTap) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap1 = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTap, kSourceDevice);

  // With touch action other than auto, tap unconfirmed is turned into tap.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&unconfirmed_tap1),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(WebInputEvent::Type::kGestureTap, unconfirmed_tap1.GetType());
  EXPECT_EQ(filter_.FilterGestureEvent(&tap),
            FilterGestureEventResult::kFiltered);
  filter_.DecreaseActiveTouches();
}

TEST_F(TouchActionFilterTest, TouchActionResetsOnResetTouchAction) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTap, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFiltered);
  filter_.DecreaseActiveTouches();

  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap),
            FilterGestureEventResult::kAllowed);
  filter_.DecreaseActiveTouches();

  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  filter_.DecreaseActiveTouches();
}

TEST_F(TouchActionFilterTest, TouchActionResetMidSequence) {
  filter_.OnHasTouchEventHandlers(true);
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGesturePinchEnd, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFiltered);

  // Even though the allowed action is auto after the reset, the remaining
  // scroll and pinch events should be suppressed.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFiltered);
  filter_.DecreaseActiveTouches();

  // A new scroll and pinch sequence should be allowed.
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kAllowed);

  // Resetting from auto to auto mid-stream should have no effect.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);
  filter_.DecreaseActiveTouches();
}

// This test makes sure that we do not reset scrolling touch action in the
// middle of a gesture sequence.
TEST_F(TouchActionFilterTest, TouchActionNotResetWithinGestureSequence) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
  filter_.OnSetTouchAction(cc::TouchAction::kPanY);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(1, 3, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(1, 5, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(cc::TouchAction::kPanY, ActiveTouchAction().value());

  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  // Simulate a touch sequence end by calling ReportAndResetTouchAction.
  filter_.DecreaseActiveTouches();
  filter_.ReportAndResetTouchAction();
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
  EXPECT_EQ(cc::TouchAction::kPanY, ActiveTouchAction().value());
  // In fling or fling boosting case, we will see ScrollUpdate after the touch
  // end.
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);
  // The |allowed_touch_action_| should have been reset, but not the
  // |scrolling_touch_action_|.
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
  EXPECT_EQ(cc::TouchAction::kPanY, ActiveTouchAction().value());
}

// The following 3 tests ensures that when the IPC message
// OnHasTouchEventHandlers is received in the middle of a gesture sequence, the
// touch action is not reset.
TEST_F(TouchActionFilterTest, OnHasTouchEventHandlersReceivedDuringTap) {
  filter_.OnHasTouchEventHandlers(false);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_TRUE(ActiveTouchAction().has_value());

  filter_.OnSetTouchAction(cc::TouchAction::kPan);
  // Simulate a simple tap gesture.
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTap, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap),
            FilterGestureEventResult::kAllowed);
  // Gesture tap indicates that there is no scroll in progress, so this should
  // reset the |allowed_touch_action_|.
  ResetTouchAction();
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
}

TEST_F(TouchActionFilterTest, OnHasTouchEventHandlersReceivedDuringDoubleTap) {
  filter_.OnHasTouchEventHandlers(false);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapCancel, kSourceDevice);
  WebGestureEvent double_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureDoubleTap, kSourceDevice);

  // Simulate a double tap gesture: GTD-->GTC-->GTD-->GTC-->GDT.
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kAuto);
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kAllowed);
  EXPECT_TRUE(ActiveTouchAction().has_value());
  filter_.OnSetTouchAction(cc::TouchAction::kPan);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kPan);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&double_tap),
            FilterGestureEventResult::kAllowed);
  filter_.DecreaseActiveTouches();
}

TEST_F(TouchActionFilterTest, OnHasTouchEventHandlersReceivedDuringScroll) {
  filter_.OnHasTouchEventHandlers(false);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapCancel, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(5, 0, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(5, 0, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

  // Simulate a gesture scroll: GTD-->GTC-->GSB-->GSU-->GSE.
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kAllowed);
  filter_.OnHasTouchEventHandlers(true);
  filter_.OnSetTouchAction(cc::TouchAction::kPan);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kPan);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);
}

// If OnHasTouchEventHandlers IPC is received after LongTap or TwoFingerTap,
// the touch action should be reset.
TEST_F(TouchActionFilterTest,
       OnHasTouchEventHandlersReceivedAfterLongTapOrTwoFingerTap) {
  filter_.OnHasTouchEventHandlers(false);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapCancel, kSourceDevice);
  WebGestureEvent long_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureLongTap, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&long_tap),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kAuto);

  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());

  filter_.OnHasTouchEventHandlers(false);
  WebGestureEvent two_finger_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTwoFingerTap, kSourceDevice);

  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&two_finger_tap),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kAuto);

  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
}

TEST_F(TouchActionFilterTest, OnHasTouchEventHandlersReceivedAfterTouchStart) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  // Receive a touch start ack, set the touch action.
  filter_.OnSetTouchAction(cc::TouchAction::kPanY);
  filter_.IncreaseActiveTouches();
  filter_.OnHasTouchEventHandlers(false);
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kPanY);
  EXPECT_EQ(filter_.allowed_touch_action().value(), cc::TouchAction::kPanY);
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kPanY);
  EXPECT_EQ(filter_.allowed_touch_action().value(), cc::TouchAction::kPanY);
}

TEST_F(TouchActionFilterTest, ResetTouchActionWithActiveTouch) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  // Receive a touch start ack, set the touch action.
  filter_.OnSetTouchAction(cc::TouchAction::kPanY);
  filter_.IncreaseActiveTouches();

  // Somehow we get the ACK for the second touch start before the ACK for the
  // first touch end.
  filter_.OnSetTouchAction(cc::TouchAction::kPan);
  filter_.IncreaseActiveTouches();

  // The first touch end comes, we report and reset touch action. The touch
  // actions should still have value.
  filter_.DecreaseActiveTouches();
  filter_.ReportAndResetTouchAction();
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kPanY);
  EXPECT_EQ(filter_.allowed_touch_action().value(), cc::TouchAction::kPanY);

  // The ack for the second touch end comes, the touch actions will be reset.
  filter_.DecreaseActiveTouches();
  filter_.ReportAndResetTouchAction();
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
}

// If the renderer is busy, the gesture event might have come before the
// OnHasTouchEventHanlders IPC is received. In this case, we should allow all
// the gestures.
TEST_F(TouchActionFilterTest, GestureArrivesBeforeHasHandlerSet) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
}

TEST_F(TouchActionFilterTest,
       PinchGesturesAllowedByCompositorAllowedTouchAction) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  // Compositor allowed touch action has a default value of Auto, and pinch
  // related gestures should be allowed.
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGesturePinchEnd, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kAllowed);
}

// Test gesture event filtering with compositor allowed touch action. It should
// test all 3 kinds of results: Allowed / Dropped / Delayed.
TEST_F(TouchActionFilterTest, FilterWithCompositorAllowedListedTouchAction) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  int dx = 2, dy = 5;
  // Test gestures that are allowed.
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(dx, dy, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

  // Vertical scroll, kInternalPanXScrolls doesn't have effect.
  filter_.OnSetCompositorAllowedTouchAction(
      cc::TouchAction::kPan | cc::TouchAction::kInternalPanXScrolls);
  EXPECT_EQ(filter_.compositor_allowed_touch_action(),
            cc::TouchAction::kPan | cc::TouchAction::kInternalPanXScrolls);
  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);

  // Don't have kInternalPanXScrolls, but this is a vertical scroll, so all the
  // events are allowed.
  ResetTouchAction();
  ResetActiveTouchAction();
  ResetCompositorAllowedTouchAction();
  filter_.OnSetCompositorAllowedTouchAction(cc::TouchAction::kPan);
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kPan);
  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);

  // Pinch related gestures are always delayed.
  ResetTouchAction();
  ResetActiveTouchAction();
  ResetCompositorAllowedTouchAction();
  filter_.OnSetCompositorAllowedTouchAction(cc::TouchAction::kPan);
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kPan);
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGesturePinchEnd, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kDelayed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kDelayed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kDelayed);

  // Scroll updates should be delayed if the compositor allowed listed touch
  // action is PanY, because there are delta along the direction that is not
  // allowed.
  ResetTouchAction();
  ResetActiveTouchAction();
  ResetCompositorAllowedTouchAction();
  filter_.OnSetCompositorAllowedTouchAction(cc::TouchAction::kPanY);
  SetNoDeferredEvents();
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kPanY);
  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kDelayed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kDelayed);

  // Horizontal scroll, don't have kInternalPanXScrolls, delay scroll events.
  ResetTouchAction();
  ResetActiveTouchAction();
  ResetCompositorAllowedTouchAction();
  filter_.OnSetCompositorAllowedTouchAction(cc::TouchAction::kPanX);
  SetNoDeferredEvents();
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kPanX);

  dy = 0;
  scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(dx, dy, kSourceDevice);
  scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      dx, dy, 0, kSourceDevice);
  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kDelayed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kDelayed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kDelayed);

  dx = 0;
  dy = 5;
  scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(dx, dy, kSourceDevice);
  scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      dx, dy, 0, kSourceDevice);
  filter_.OnSetCompositorAllowedTouchAction(cc::TouchAction::kPanX);
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kPanX);
  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kDelayed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kDelayed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kDelayed);
}

TEST_F(TouchActionFilterTest, CompositorAllowedTouchActionResetToAuto) {
  filter_.OnHasTouchEventHandlers(true);

  filter_.OnSetCompositorAllowedTouchAction(cc::TouchAction::kPan);
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kPan);
  ResetTouchAction();
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kAuto);
}

TEST_F(TouchActionFilterTest, CompositorAllowedTouchActionAutoNoHasHandlers) {
  filter_.OnHasTouchEventHandlers(false);
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kAuto);

  ResetTouchAction();
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kAuto);
}

TEST_F(TouchActionFilterTest, ResetBeforeHasHandlerSet) {
  // This should not crash, and should set touch action to auto.
  ResetTouchAction();
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
}

TEST_F(TouchActionFilterTest,
       CompositorAllowedTouchActionNotResetAtGestureScrollEnd) {
  filter_.OnHasTouchEventHandlers(true);

  filter_.OnSetCompositorAllowedTouchAction(cc::TouchAction::kPan);
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kPan);

  int dx = 2, dy = 5;
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(dx, dy, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);

  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);

  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kPan);
}

// Having a gesture scroll begin without tap down should assume touch action is
// auto;
TEST_F(TouchActionFilterTest, ScrollBeginWithoutTapDown) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(5, 0, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.compositor_allowed_touch_action(), cc::TouchAction::kAuto);
}

// This tests a gesture tap down with |num_of_active_touches_| == 0
TEST_F(TouchActionFilterTest, TapDownWithZeroNumOfActiveTouches) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureTapDown, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kAllowed);
  EXPECT_TRUE(ActiveTouchAction().has_value());
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kAuto);
}

// Regression test for crbug.com/771330. One can start one finger panning y, and
// add another finger to pinch zooming. The pinch zooming should not be allowed
// if the allowed touch action doesn't allow it.
TEST_F(TouchActionFilterTest, PinchZoomStartsWithOneFingerPanDisallowed) {
  filter_.OnHasTouchEventHandlers(true);
  filter_.OnSetTouchAction(cc::TouchAction::kPanY);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(0, 3, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(5, 10, 0,
                                                         kSourceDevice);
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGesturePinchEnd, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::Type::kGestureScrollEnd, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kAllowed);
}

TEST_F(TouchActionFilterTest, ScrollBeginWithoutTapDownWithKnownTouchAction) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  filter_.OnSetTouchAction(cc::TouchAction::kPan);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(5, 0, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::TouchAction::kPan);
  EXPECT_EQ(filter_.allowed_touch_action().value(), cc::TouchAction::kPan);
}

TEST_F(TouchActionFilterTest, TouchpadScroll) {
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(
          2, 3, blink::WebGestureDevice::kTouchpad);

  // cc::TouchAction::kNone filters out only touchscreen scroll events.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::TouchAction::kNone);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kAllowed);
}

}  // namespace input

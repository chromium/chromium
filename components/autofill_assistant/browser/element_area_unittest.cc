// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/element_area.h"

#include <algorithm>
#include <map>
#include <ostream>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;

namespace autofill_assistant {

// User-friendly RectF string representation for matchers.
//
// operator<< must not be in an anonymous namespace to be usable in all
// matchers.
std::string ToString(const RectF& rect) {
  return base::StringPrintf("RectF(%2.2f, %2.2f, %2.2f, %2.2f)", rect.left,
                            rect.top, rect.right, rect.bottom);
}

std::ostream& operator<<(std::ostream& out, const RectF& rectf) {
  return out << ToString(rectf);
}

namespace {

MATCHER_P4(MatchingRectF,
           left,
           top,
           right,
           bottom,
           ToString(RectF{left, top, right, bottom})) {
  if (abs(left - arg.left) < 0.01 && abs(top - arg.top) < 0.01 &&
      abs(right - arg.right) < 0.01 && abs(bottom - arg.bottom) < 0.01) {
    return true;
  }
  *result_listener << arg;
  return false;
}

MATCHER(EmptyRectF, "EmptyRectF") {
  if (arg.empty())
    return true;

  *result_listener << arg;
  return false;
}

ACTION(DoNothing) {}

class ElementAreaTest : public testing::Test {
 protected:
  ElementAreaTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        element_area_(&delegate_) {
    delegate_.SetWebController(&mock_web_controller_);
    delegate_.GetMutableSettings()->element_position_update_interval =
        base::TimeDelta::FromMilliseconds(100);

    ON_CALL(mock_web_controller_, OnGetElementPosition(_, _))
        .WillByDefault(RunOnceCallback<1>(false, RectF()));
    ON_CALL(mock_web_controller_, OnGetVisualViewport(_))
        .WillByDefault(RunOnceCallback<0>(true, RectF(0, 0, 200, 400)));

    element_area_.SetOnUpdate(base::BindRepeating(&ElementAreaTest::OnUpdate,
                                                  base::Unretained(this)));
  }

  void SetElement(const std::string& selector) {
    SetElement(selector, /* restricted= */ false);
  }

  void SetElement(const std::string& selector, bool restricted) {
    ElementAreaProto area;
    auto* rectangle = restricted ? area.add_restricted() : area.add_touchable();
    rectangle->add_elements()->add_selectors(selector);
    element_area_.SetFromProto(area);
  }

  void OnUpdate(const RectF& visual_viewport,
                const std::vector<RectF>& touchable_area,
                const std::vector<RectF>& restricted_area) {
    on_update_call_count_++;
    reported_visual_viewport_ = visual_viewport;
    reported_area_ = touchable_area;
    reported_restricted_area_ = restricted_area;
  }

  // task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_environment_;

  MockWebController mock_web_controller_;
  FakeScriptExecutorDelegate delegate_;
  ElementArea element_area_;
  int on_update_call_count_ = 0;
  RectF reported_visual_viewport_;
  std::vector<RectF> reported_area_;
  std::vector<RectF> reported_restricted_area_;
};

TEST_F(ElementAreaTest, Empty) {
  EXPECT_THAT(reported_area_, IsEmpty());

  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);
  EXPECT_THAT(rectangles, IsEmpty());

  RectF viewport;
  element_area_.GetVisualViewport(&viewport);
  EXPECT_THAT(viewport, EmptyRectF());
}

TEST_F(ElementAreaTest, ElementNotFound) {
  SetElement("#not_found");
  EXPECT_THAT(reported_area_, ElementsAre(EmptyRectF()));

  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);
  EXPECT_THAT(rectangles, ElementsAre(EmptyRectF()));
}

TEST_F(ElementAreaTest, GetVisualViewport) {
  SetElement("#some_element");
  RectF viewport;
  element_area_.GetVisualViewport(&viewport);
  EXPECT_THAT(viewport, MatchingRectF(0, 0, 200, 400));
}

TEST_F(ElementAreaTest, OneRectangle) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#found"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(25, 25, 75, 75)));

  SetElement("#found");
  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);
  EXPECT_THAT(rectangles, ElementsAre(MatchingRectF(25, 25, 75, 75)));
}

TEST_F(ElementAreaTest, CallOnUpdate) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#found"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(25, 25, 75, 75)));

  SetElement("#found");
  EXPECT_EQ(on_update_call_count_, 1);
  EXPECT_THAT(reported_visual_viewport_, MatchingRectF(0, 0, 200, 400));
  EXPECT_THAT(reported_area_, ElementsAre(MatchingRectF(25, 25, 75, 75)));
}

TEST_F(ElementAreaTest, DontCallOnUpdateWhenViewportMissing) {
  // Swallowing calls to OnGetVisualViewport guarantees that the viewport
  // position will never be known.
  EXPECT_CALL(mock_web_controller_, OnGetVisualViewport(_))
      .WillOnce(DoNothing());
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(Eq(Selector({"#found"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(25, 25, 75, 75)));

  SetElement("#found");
  EXPECT_EQ(on_update_call_count_, 0);
}

TEST_F(ElementAreaTest, CallOnUpdateWhenViewportMissingAndEmptyRect) {
  EXPECT_CALL(mock_web_controller_, OnGetVisualViewport(_))
      .WillRepeatedly(RunOnceCallback<0>(false, RectF()));

  SetElement("#found");

  // A newly empty element area should be reported.
  on_update_call_count_ = 0;
  element_area_.Clear();

  EXPECT_EQ(on_update_call_count_, 1);
  EXPECT_THAT(reported_visual_viewport_, EmptyRectF());
  EXPECT_THAT(reported_area_, IsEmpty());
}

TEST_F(ElementAreaTest, TwoRectangles) {
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#top_left"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0, 0, 25, 25)));
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#bottom_right"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(25, 25, 100, 100)));

  ElementAreaProto area_proto;
  area_proto.add_touchable()->add_elements()->add_selectors("#top_left");
  area_proto.add_touchable()->add_elements()->add_selectors("#bottom_right");
  element_area_.SetFromProto(area_proto);

  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);
  EXPECT_THAT(rectangles, ElementsAre(MatchingRectF(0, 0, 25, 25),
                                      MatchingRectF(25, 25, 100, 100)));
}

TEST_F(ElementAreaTest, OneRectangleTwoElements) {
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element1"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(1, 3, 2, 4)));
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element2"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(5, 2, 6, 5)));

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_touchable();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  element_area_.SetFromProto(area_proto);

  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);
  EXPECT_THAT(rectangles, ElementsAre(MatchingRectF(1, 2, 6, 5)));
}

TEST_F(ElementAreaTest, DoNotReportIncompleteRectangles) {
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element1"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(1, 3, 2, 4)));

  // Getting the position of #element2 neither succeeds nor fails, simulating an
  // intermediate state which shouldn't be reported to the callback.
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element2"}).MustBeVisible()), _))
      .WillOnce(DoNothing());  // overrides default action

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_touchable();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  element_area_.SetFromProto(area_proto);

  EXPECT_THAT(reported_area_, IsEmpty());

  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);
  EXPECT_THAT(rectangles, ElementsAre(MatchingRectF(1, 3, 2, 4)));
}

TEST_F(ElementAreaTest, OneRectangleFourElements) {
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element1"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0, 0, 1, 1)));
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element2"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(9, 9, 100, 100)));
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element3"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0, 9, 1, 100)));
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element4"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(9, 0, 100, 1)));

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_touchable();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  rectangle_proto->add_elements()->add_selectors("#element3");
  rectangle_proto->add_elements()->add_selectors("#element4");
  element_area_.SetFromProto(area_proto);

  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);
  EXPECT_THAT(rectangles, ElementsAre(MatchingRectF(0, 0, 100, 100)));
}

TEST_F(ElementAreaTest, OneRectangleMissingElementsReported) {
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element1"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(1, 1, 2, 2)));
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element2"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(false, RectF()));

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_touchable();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  element_area_.SetFromProto(area_proto);

  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);
  EXPECT_THAT(rectangles, ElementsAre(MatchingRectF(1, 1, 2, 2)));

  EXPECT_THAT(reported_area_, ElementsAre(MatchingRectF(1, 1, 2, 2)));
}

TEST_F(ElementAreaTest, FullWidthRectangle) {
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element1"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(1, 3, 2, 4)));
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element2"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(5, 7, 6, 8)));
  EXPECT_CALL(mock_web_controller_, OnGetVisualViewport(_))
      .WillRepeatedly(RunOnceCallback<0>(true, RectF(100, 0, 200, 400)));

  ElementAreaProto area_proto;
  auto* rectangle_proto = area_proto.add_touchable();
  rectangle_proto->add_elements()->add_selectors("#element1");
  rectangle_proto->add_elements()->add_selectors("#element2");
  rectangle_proto->set_full_width(true);
  element_area_.SetFromProto(area_proto);

  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);

  // left and right of the box come from the visual viewport, top from the 1st
  // element, bottom from the 2nd.
  EXPECT_THAT(rectangles, ElementsAre(MatchingRectF(100, 3, 200, 8)));
}

TEST_F(ElementAreaTest, ElementMovesAfterUpdate) {
  testing::InSequence seq;
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0, 25, 100, 50)))
      .WillOnce(RunOnceCallback<1>(true, RectF(0, 50, 100, 75)));

  SetElement("#element");

  std::vector<RectF> original;
  element_area_.GetTouchableRectangles(&original);
  EXPECT_THAT(original, ElementsAre(MatchingRectF(0, 25, 100, 50)));
  EXPECT_THAT(reported_area_, ElementsAre(MatchingRectF(0, 25, 100, 50)));

  element_area_.Update();

  // Updated area is available
  std::vector<RectF> updated;
  element_area_.GetTouchableRectangles(&updated);
  EXPECT_THAT(updated, ElementsAre(MatchingRectF(0, 50, 100, 75)));

  // Updated area is reported
  EXPECT_THAT(reported_area_, ElementsAre(MatchingRectF(0, 50, 100, 75)));
}

TEST_F(ElementAreaTest, ElementMovesWithTime) {
  testing::InSequence seq;
  EXPECT_CALL(
      mock_web_controller_,
      OnGetElementPosition(Eq(Selector({"#element"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(0, 25, 100, 50)))
      .WillRepeatedly(RunOnceCallback<1>(true, RectF(0, 50, 100, 75)));

  SetElement("#element");

  EXPECT_THAT(reported_area_, ElementsAre(MatchingRectF(0, 25, 100, 50)));

  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));

  // Updated area is available
  std::vector<RectF> rectangles;
  element_area_.GetTouchableRectangles(&rectangles);
  EXPECT_THAT(rectangles, ElementsAre(MatchingRectF(0, 50, 100, 75)));

  // Updated area is reported
  EXPECT_THAT(reported_area_, ElementsAre(MatchingRectF(0, 50, 100, 75)));
}

TEST_F(ElementAreaTest, RestrictedElement) {
  EXPECT_CALL(mock_web_controller_,
              OnGetElementPosition(
                  Eq(Selector({"#restricted_element"}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<1>(true, RectF(25, 25, 75, 75)));

  SetElement("#restricted_element", /* restricted= */ true);

  EXPECT_EQ(on_update_call_count_, 1);
  EXPECT_THAT(reported_area_, IsEmpty());
  EXPECT_THAT(reported_restricted_area_,
              ElementsAre(MatchingRectF(25, 25, 75, 75)));

  std::vector<RectF> touchable_rectangles;
  std::vector<RectF> restricted_rectangles;
  element_area_.GetTouchableRectangles(&touchable_rectangles);
  element_area_.GetRestrictedRectangles(&restricted_rectangles);

  EXPECT_THAT(touchable_rectangles, IsEmpty());
  EXPECT_THAT(restricted_rectangles,
              ElementsAre(MatchingRectF(25, 25, 75, 75)));
}
}  // namespace
}  // namespace autofill_assistant

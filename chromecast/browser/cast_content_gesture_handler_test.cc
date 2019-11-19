// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_gesture_handler.h"

#include "base/memory/weak_ptr.h"
#include "chromecast/base/chromecast_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"

// Gmock matchers and actions that we use below.
using testing::_;
using testing::AnyOf;
using testing::Eq;
using testing::Return;
using testing::SetArgPointee;
using testing::WithArg;

namespace chromecast {

namespace {

constexpr gfx::Point kLeftSidePoint(5, 50);
constexpr gfx::Point kOngoingBackGesturePoint1(70, 50);
constexpr gfx::Point kOngoingBackGesturePoint2(75, 50);
constexpr gfx::Point kValidBackGestureEndPoint(90, 50);
constexpr gfx::Point kPastTheEndPoint1(105, 50);
constexpr gfx::Point kPastTheEndPoint2(200, 50);

constexpr gfx::Point kTopSidePoint(100, 5);
constexpr gfx::Point kOngoingTopGesturePoint1(100, 70);
constexpr gfx::Point kOngoingTopGesturePoint2(100, 75);
constexpr gfx::Point kTopGestureEndPoint(100, 90);

constexpr gfx::Point kRightSidePoint(500, 50);
constexpr gfx::Point kOngoingRightGesturePoint1(400, 50);
constexpr gfx::Point kRightGestureEndPoint(200, 60);

}  // namespace

class MockCastContentWindowDelegate
    : public base::SupportsWeakPtr<MockCastContentWindowDelegate>,
      public CastContentWindow::Delegate {
 public:
  ~MockCastContentWindowDelegate() override = default;

  MOCK_METHOD1(CanHandleGesture, bool(GestureType gesture_type));
  MOCK_METHOD1(ConsumeGesture, bool(GestureType gesture_type));
  MOCK_METHOD2(CancelGesture,
               void(GestureType gesture_type,
                    const gfx::Point& touch_location));
  MOCK_METHOD2(GestureProgress,
               void(GestureType gesture_type,
                    const gfx::Point& touch_location));
  std::string GetId() override { return "mockContentWindowDelegate"; }
};

class CastContentGestureHandlerTest : public testing::Test {
 public:
  CastContentGestureHandlerTest() : dispatcher_(delegate_.AsWeakPtr(), true) {}

 protected:
  MockCastContentWindowDelegate delegate_;
  CastContentGestureHandler dispatcher_;
};

// Verify the simple case of a left swipe with the right horizontal leads to
// back.
TEST_F(CastContentGestureHandlerTest, VerifySimpleBackSuccess) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(delegate_, ConsumeGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::LEFT,
                              kOngoingBackGesturePoint1);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::END,
                              CastSideSwipeOrigin::LEFT,
                              kValidBackGestureEndPoint);
}

// Verify that if the finger is not lifted, that's not a back gesture.
TEST_F(CastContentGestureHandlerTest, VerifyNoDispatchOnNoLift) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, ConsumeGesture(Eq(GestureType::GO_BACK))).Times(0);
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kValidBackGestureEndPoint)));
  EXPECT_CALL(delegate_,
              GestureProgress(Eq(GestureType::GO_BACK), Eq(kPastTheEndPoint1)));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::LEFT,
                              kValidBackGestureEndPoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::LEFT, kPastTheEndPoint1);
}

// Verify that multiple 'continue' events still only lead to one back
// invocation.
TEST_F(CastContentGestureHandlerTest, VerifyOnlySingleDispatch) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kValidBackGestureEndPoint)));
  EXPECT_CALL(delegate_,
              GestureProgress(Eq(GestureType::GO_BACK), Eq(kPastTheEndPoint1)));
  EXPECT_CALL(delegate_, ConsumeGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::LEFT,
                              kValidBackGestureEndPoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::LEFT, kPastTheEndPoint1);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::END,
                              CastSideSwipeOrigin::LEFT, kPastTheEndPoint2);
}

// Verify that if the delegate says it doesn't handle back that we won't try to
// ask them to consume it.
TEST_F(CastContentGestureHandlerTest, VerifyDelegateDoesNotConsumeUnwanted) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(false));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::LEFT,
                              kValidBackGestureEndPoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::END,
                              CastSideSwipeOrigin::LEFT, kPastTheEndPoint2);
}

// Verify that a not-left gesture doesn't lead to a swipe.
TEST_F(CastContentGestureHandlerTest, VerifyNotLeftSwipeIsNotBack) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::TOP);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              CastSideSwipeOrigin::TOP, kTopSidePoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::TOP,
                              kOngoingTopGesturePoint2);
}

// Verify that if the gesture doesn't go far enough horizontally that we will
// not consider it a swipe.
TEST_F(CastContentGestureHandlerTest, VerifyNotFarEnoughRightIsNotBack) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(delegate_, CancelGesture(Eq(GestureType::GO_BACK),
                                       Eq(kOngoingBackGesturePoint2)));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::LEFT,
                              kOngoingBackGesturePoint1);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::END,
                              CastSideSwipeOrigin::LEFT,
                              kOngoingBackGesturePoint2);
}

// Verify that if the gesture ends before going far enough, that's also not a
// swipe.
TEST_F(CastContentGestureHandlerTest, VerifyNotFarEnoughRightAndEndIsNotBack) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::GO_BACK),
                                         Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(delegate_, CancelGesture(Eq(GestureType::GO_BACK),
                                       Eq(kOngoingBackGesturePoint2)));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::LEFT);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::LEFT,
                              kOngoingBackGesturePoint1);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::END,
                              CastSideSwipeOrigin::LEFT,
                              kOngoingBackGesturePoint2);
}

// Verify simple top-down drag.
TEST_F(CastContentGestureHandlerTest, VerifySimpleTopSuccess) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::TOP_DRAG),
                                         Eq(kOngoingTopGesturePoint1)));
  EXPECT_CALL(delegate_, ConsumeGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(true));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::TOP);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              CastSideSwipeOrigin::TOP, kTopSidePoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::TOP,
                              kOngoingTopGesturePoint1);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::END,
                              CastSideSwipeOrigin::LEFT, kTopGestureEndPoint);
}

// Verify simple right-to-left drag.
TEST_F(CastContentGestureHandlerTest, VerifySimpleRightSuccess) {
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::RIGHT_DRAG)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::GO_BACK)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(delegate_, CanHandleGesture(Eq(GestureType::TOP_DRAG)))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(delegate_, GestureProgress(Eq(GestureType::RIGHT_DRAG),
                                         Eq(kOngoingRightGesturePoint1)));
  EXPECT_CALL(delegate_, ConsumeGesture(Eq(GestureType::RIGHT_DRAG)))
      .WillRepeatedly(Return(true));
  dispatcher_.CanHandleSwipe(CastSideSwipeOrigin::RIGHT);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              CastSideSwipeOrigin::RIGHT, kRightSidePoint);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              CastSideSwipeOrigin::RIGHT,
                              kOngoingRightGesturePoint1);
  dispatcher_.HandleSideSwipe(CastSideSwipeEvent::END,
                              CastSideSwipeOrigin::LEFT, kRightGestureEndPoint);
}

}  // namespace chromecast

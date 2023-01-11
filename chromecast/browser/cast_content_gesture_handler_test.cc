// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_gesture_handler.h"

#include "base/functional/callback_helpers.h"
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

class MockGestureHandler : public mojom::GestureHandler {
 public:
  MockGestureHandler() = default;
  ~MockGestureHandler() override = default;

  MOCK_METHOD(void, OnBackGesture, (OnBackGestureCallback), (override));
  MOCK_METHOD(void, OnBackGestureProgress, (const gfx::Point&), (override));
  MOCK_METHOD(void, OnTopDragGestureProgress, (const gfx::Point&), (override));
  MOCK_METHOD(void, OnTopDragGestureDone, (), (override));
  MOCK_METHOD(void,
              OnRightDragGestureProgress,
              (const gfx::Point&),
              (override));
  MOCK_METHOD(void, OnRightDragGestureDone, (), (override));
  MOCK_METHOD(void, OnBackGestureCancel, (), (override));
  MOCK_METHOD(void, OnTapGesture, (), (override));
  MOCK_METHOD(void, OnTapDownGesture, (), (override));
};

}  // namespace

class CastContentGestureHandlerTest : public testing::Test {
 public:
  CastContentGestureHandlerTest() {}

  void SetUp() final {
    gesture_router_ = std::make_unique<GestureRouter>();
    dispatcher_ = std::make_unique<CastContentGestureHandler>(
        gesture_router_.get(), true);
    gesture_router_->SetHandler(&handler_);
  }

 protected:
  std::unique_ptr<GestureRouter> gesture_router_;
  std::unique_ptr<CastContentGestureHandler> dispatcher_;
  MockGestureHandler handler_;
};

// Verify the simple case of a left swipe with the right horizontal leads to
// back.
TEST_F(CastContentGestureHandlerTest, VerifySimpleBackSuccess) {
  gesture_router_->SetCanTopDrag(false);
  gesture_router_->SetCanGoBack(true);
  EXPECT_CALL(handler_, OnBackGestureProgress(Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(handler_, OnBackGesture(_));
  ASSERT_TRUE(dispatcher_->CanHandleSwipe(CastSideSwipeOrigin::LEFT));
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                               CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::LEFT,
                               kOngoingBackGesturePoint1);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::END,
                               CastSideSwipeOrigin::LEFT,
                               kValidBackGestureEndPoint);
}

// Verify that if the finger is not lifted, that's not a back gesture.
TEST_F(CastContentGestureHandlerTest, VerifyNoDispatchOnNoLift) {
  gesture_router_->SetCanTopDrag(false);
  gesture_router_->SetCanGoBack(true);
  EXPECT_CALL(handler_, OnBackGestureProgress(Eq(kValidBackGestureEndPoint)));
  EXPECT_CALL(handler_, OnBackGesture(_)).Times(0);
  EXPECT_CALL(handler_, OnBackGestureProgress(Eq(kPastTheEndPoint1)));
  ASSERT_TRUE(dispatcher_->CanHandleSwipe(CastSideSwipeOrigin::LEFT));
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                               CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::LEFT,
                               kValidBackGestureEndPoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::LEFT, kPastTheEndPoint1);
}

// Verify that multiple 'continue' events still only lead to one back
// invocation.
TEST_F(CastContentGestureHandlerTest, VerifyOnlySingleDispatch) {
  gesture_router_->SetCanTopDrag(false);
  gesture_router_->SetCanGoBack(true);

  EXPECT_CALL(handler_, OnBackGestureProgress(Eq(kValidBackGestureEndPoint)));
  EXPECT_CALL(handler_, OnBackGestureProgress(Eq(kPastTheEndPoint1)));
  EXPECT_CALL(handler_, OnBackGesture(_));
  ASSERT_TRUE(dispatcher_->CanHandleSwipe(CastSideSwipeOrigin::LEFT));
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                               CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::LEFT,
                               kValidBackGestureEndPoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::LEFT, kPastTheEndPoint1);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::END,
                               CastSideSwipeOrigin::LEFT, kPastTheEndPoint2);
}

// Verify that if the delegate says it doesn't handle back that we won't try to
// ask them to consume it.
TEST_F(CastContentGestureHandlerTest, VerifyDelegateDoesNotConsumeUnwanted) {
  gesture_router_->SetCanTopDrag(false);
  gesture_router_->SetCanGoBack(false);
  ASSERT_FALSE(dispatcher_->CanHandleSwipe(CastSideSwipeOrigin::LEFT));
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                               CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::LEFT,
                               kValidBackGestureEndPoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::END,
                               CastSideSwipeOrigin::LEFT, kPastTheEndPoint2);
}

// Verify that a not-left gesture doesn't lead to a swipe.
TEST_F(CastContentGestureHandlerTest, VerifyNotLeftSwipeIsNotBack) {
  gesture_router_->SetCanTopDrag(false);

  ASSERT_FALSE(dispatcher_->CanHandleSwipe(CastSideSwipeOrigin::TOP));
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                               CastSideSwipeOrigin::TOP, kTopSidePoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::TOP,
                               kOngoingTopGesturePoint2);
}

// Verify that if the gesture doesn't go far enough horizontally that we will
// not consider it a swipe.
TEST_F(CastContentGestureHandlerTest, VerifyNotFarEnoughRightIsNotBack) {
  gesture_router_->SetCanTopDrag(false);
  gesture_router_->SetCanGoBack(true);

  EXPECT_CALL(handler_, OnBackGestureProgress(Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(handler_, OnBackGestureCancel());
  ASSERT_TRUE(dispatcher_->CanHandleSwipe(CastSideSwipeOrigin::LEFT));
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                               CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::LEFT,
                               kOngoingBackGesturePoint1);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::END,
                               CastSideSwipeOrigin::LEFT,
                               kOngoingBackGesturePoint2);
}

// Verify that if the gesture ends before going far enough, that's also not a
// swipe.
TEST_F(CastContentGestureHandlerTest, VerifyNotFarEnoughRightAndEndIsNotBack) {
  gesture_router_->SetCanTopDrag(false);
  gesture_router_->SetCanGoBack(true);

  EXPECT_CALL(handler_, OnBackGestureProgress(Eq(kOngoingBackGesturePoint1)));
  EXPECT_CALL(handler_, OnBackGestureCancel());
  ASSERT_TRUE(dispatcher_->CanHandleSwipe(CastSideSwipeOrigin::LEFT));
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                               CastSideSwipeOrigin::LEFT, kLeftSidePoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::LEFT,
                               kOngoingBackGesturePoint1);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::END,
                               CastSideSwipeOrigin::LEFT,
                               kOngoingBackGesturePoint2);
}

// Verify simple top-down drag.
TEST_F(CastContentGestureHandlerTest, VerifySimpleTopSuccess) {
  gesture_router_->SetCanTopDrag(true);
  gesture_router_->SetCanGoBack(false);

  EXPECT_CALL(handler_, OnTopDragGestureProgress(Eq(kOngoingTopGesturePoint1)));
  EXPECT_CALL(handler_, OnTopDragGestureDone());
  ASSERT_TRUE(dispatcher_->CanHandleSwipe(CastSideSwipeOrigin::TOP));
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                               CastSideSwipeOrigin::TOP, kTopSidePoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::TOP,
                               kOngoingTopGesturePoint1);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::END,
                               CastSideSwipeOrigin::TOP, kTopGestureEndPoint);
}

// Verify simple right-to-left drag.
TEST_F(CastContentGestureHandlerTest, VerifySimpleRightSuccess) {
  gesture_router_->SetCanRightDrag(true);
  gesture_router_->SetCanTopDrag(false);
  gesture_router_->SetCanGoBack(false);

  EXPECT_CALL(handler_,
              OnRightDragGestureProgress(Eq(kOngoingRightGesturePoint1)));
  EXPECT_CALL(handler_, OnRightDragGestureDone());
  ASSERT_TRUE(dispatcher_->CanHandleSwipe(CastSideSwipeOrigin::RIGHT));
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                               CastSideSwipeOrigin::RIGHT, kRightSidePoint);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                               CastSideSwipeOrigin::RIGHT,
                               kOngoingRightGesturePoint1);
  dispatcher_->HandleSideSwipe(CastSideSwipeEvent::END,
                               CastSideSwipeOrigin::RIGHT,
                               kRightGestureEndPoint);
}

}  // namespace chromecast

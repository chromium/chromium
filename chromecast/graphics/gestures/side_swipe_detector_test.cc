// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/side_swipe_detector.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

// Gmock matchers and actions that we use below.
using testing::_;
using testing::AnyOf;
using testing::Eq;
using testing::Return;

namespace chromecast {
namespace test {

namespace {

constexpr base::TimeDelta kTimeDelay = base::Milliseconds(100);
constexpr int kSwipeDistance = 50;
constexpr int kNumSteps = 5;
// constexpr gfx::Point kZeroPoint{0, 0};

}  // namespace

class MockCastGestureHandler : public CastGestureHandler {
 public:
  ~MockCastGestureHandler() override = default;

  MOCK_METHOD0(GetPriority, Priority());
  MOCK_METHOD1(CanHandleSwipe, bool(CastSideSwipeOrigin origin));
  MOCK_METHOD3(HandleSideSwipe,
               void(CastSideSwipeEvent event,
                    CastSideSwipeOrigin swipe_origin,
                    const gfx::Point& touch_location));
  MOCK_METHOD1(HandleTapDownGesture, void(const gfx::Point& touch_location));
  MOCK_METHOD1(HandleTapGesture, void(const gfx::Point& touch_location));
};

// Event sink to check for events that get through (or don't get through) after
// the system gesture handler handles them.
class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() : EventHandler(), num_touch_events_received_(0) {}

  void OnTouchEvent(ui::TouchEvent* event) override {
    num_touch_events_received_++;
  }

  int NumTouchEventsReceived() const { return num_touch_events_received_; }

 private:
  int num_touch_events_received_;
};

class SideSwipeDetectorTest : public aura::test::AuraTestBase {
 public:
  ~SideSwipeDetectorTest() override = default;

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();

    gesture_handler_ = std::make_unique<MockCastGestureHandler>();
    side_swipe_detector_ = std::make_unique<SideSwipeDetector>(
        gesture_handler_.get(), root_window());
    test_event_handler_ = std::make_unique<TestEventHandler>();
    root_window()->AddPostTargetHandler(test_event_handler_.get());

    mock_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto mock_timer = std::make_unique<base::OneShotTimer>(
        mock_task_runner_->GetMockTickClock());
    mock_timer->SetTaskRunner(mock_task_runner_);
  }

  void TearDown() override {
    side_swipe_detector_.reset();
    gesture_handler_.reset();

    aura::test::AuraTestBase::TearDown();
  }

  void Drag(const gfx::Point& start_point,
            const base::TimeDelta& start_hold_time,
            const base::TimeDelta& drag_time,
            const gfx::Point& end_point,
            ui::PointerId pointer_id,
            bool end_release = true) {
    ui::TouchEvent press(
        ui::EventType::kTouchPressed, start_point, mock_clock()->NowTicks(),
        ui::PointerDetails(ui::EventPointerType::kTouch, pointer_id));
    GetEventGenerator().Dispatch(&press);
    mock_task_runner()->AdvanceMockTickClock(start_hold_time);
    mock_task_runner()->FastForwardBy(start_hold_time);

    ui::TouchEvent move(
        ui::EventType::kTouchMoved, end_point, mock_clock()->NowTicks(),
        ui::PointerDetails(ui::EventPointerType::kTouch, pointer_id));
    GetEventGenerator().Dispatch(&move);
    mock_task_runner()->AdvanceMockTickClock(drag_time);
    mock_task_runner()->FastForwardBy(drag_time);

    if (end_release) {
      ui::TouchEvent release(
          ui::EventType::kTouchReleased, end_point, mock_clock()->NowTicks(),
          ui::PointerDetails(ui::EventPointerType::kTouch, pointer_id));
      GetEventGenerator().Dispatch(&release);
    }
  }

  ui::test::EventGenerator& GetEventGenerator() {
    if (!event_generator_) {
      event_generator_ =
          std::make_unique<ui::test::EventGenerator>(root_window());
    }
    return *event_generator_.get();
  }

  MockCastGestureHandler& mock_gesture_handler() { return *gesture_handler_; }

  base::TestMockTimeTaskRunner* mock_task_runner() const {
    return mock_task_runner_.get();
  }

  const base::TickClock* mock_clock() const {
    return mock_task_runner_->GetMockTickClock();
  }

  TestEventHandler& test_event_handler() { return *test_event_handler_; }

 private:
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;

  std::unique_ptr<SideSwipeDetector> side_swipe_detector_;
  std::unique_ptr<TestEventHandler> test_event_handler_;
  std::unique_ptr<MockCastGestureHandler> gesture_handler_;
};

// Test that initialization works and initial state is clean.
TEST_F(SideSwipeDetectorTest, Initialization) {
  EXPECT_CALL(mock_gesture_handler(), CanHandleSwipe(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_gesture_handler(), HandleSideSwipe(_, _, _)).Times(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

// A swipe in the middle of the screen should produce no system gesture.
TEST_F(SideSwipeDetectorTest, SwipeWithNoSystemGesture) {
  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height() / 2);
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point,
                                  drag_point - gfx::Vector2d(0, kSwipeDistance),
                                  kTimeDelay, kNumSteps);

  EXPECT_CALL(mock_gesture_handler(), CanHandleSwipe(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_gesture_handler(), HandleSideSwipe(_, _, _)).Times(0);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, SwipeFromLeft) {
  gfx::Point drag_point(0, root_window()->bounds().height() / 2);
  auto end_point = drag_point + gfx::Vector2d(kSwipeDistance, 0);

  EXPECT_CALL(mock_gesture_handler(),
              CanHandleSwipe(Eq(CastSideSwipeOrigin::LEFT)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              Eq(CastSideSwipeOrigin::LEFT), drag_point))
      .Times(1);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              Eq(CastSideSwipeOrigin::LEFT), _))
      .Times(kNumSteps);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::END,
                              Eq(CastSideSwipeOrigin::LEFT), end_point))
      .Times(1);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point, end_point, kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, SwipeFromRight) {
  gfx::Point drag_point(root_window()->bounds().width(),
                        root_window()->bounds().height() / 2);
  auto end_point = drag_point - gfx::Vector2d(kSwipeDistance, 0);

  EXPECT_CALL(mock_gesture_handler(),
              CanHandleSwipe(Eq(CastSideSwipeOrigin::RIGHT)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              Eq(CastSideSwipeOrigin::RIGHT), drag_point))
      .Times(1);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              Eq(CastSideSwipeOrigin::RIGHT), _))
      .Times(kNumSteps);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::END,
                              Eq(CastSideSwipeOrigin::RIGHT), end_point))
      .Times(1);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point, end_point, kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, SwipeFromTop) {
  gfx::Point drag_point(root_window()->bounds().width() / 2, 0);
  auto end_point = drag_point + gfx::Vector2d(0, kSwipeDistance);

  EXPECT_CALL(mock_gesture_handler(),
              CanHandleSwipe(Eq(CastSideSwipeOrigin::TOP)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              Eq(CastSideSwipeOrigin::TOP), drag_point))
      .Times(1);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              Eq(CastSideSwipeOrigin::TOP), _))
      .Times(kNumSteps);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::END,
                              Eq(CastSideSwipeOrigin::TOP), end_point))
      .Times(1);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point, end_point, kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, SwipeFromBottom) {
  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height());
  auto end_point = drag_point - gfx::Vector2d(0, kSwipeDistance);

  EXPECT_CALL(mock_gesture_handler(),
              CanHandleSwipe(Eq(CastSideSwipeOrigin::BOTTOM)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              Eq(CastSideSwipeOrigin::BOTTOM), drag_point))
      .Times(1);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              Eq(CastSideSwipeOrigin::BOTTOM), _))
      .Times(kNumSteps);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::END,
                              Eq(CastSideSwipeOrigin::BOTTOM), end_point))
      .Times(1);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point, end_point, kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

TEST_F(SideSwipeDetectorTest, SwipeUnhandledIgnored) {
  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height());
  auto end_point = drag_point - gfx::Vector2d(0, kSwipeDistance);

  EXPECT_CALL(mock_gesture_handler(),
              CanHandleSwipe(Eq(CastSideSwipeOrigin::BOTTOM)))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              Eq(CastSideSwipeOrigin::BOTTOM), drag_point))
      .Times(0);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              Eq(CastSideSwipeOrigin::BOTTOM), _))
      .Times(0);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::END,
                              Eq(CastSideSwipeOrigin::BOTTOM), end_point))
      .Times(0);

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.GestureScrollSequence(drag_point, end_point, kTimeDelay, kNumSteps);

  base::RunLoop().RunUntilIdle();
  EXPECT_NE(0, test_event_handler().NumTouchEventsReceived());
}

// Test that a second gesture while the first is still in process will be
// ignored.
TEST_F(SideSwipeDetectorTest, IgnoreSecondFinger) {
  gfx::Point drag_point(root_window()->bounds().width() / 2,
                        root_window()->bounds().height());
  auto end_point = drag_point - gfx::Vector2d(0, kSwipeDistance);

  EXPECT_CALL(mock_gesture_handler(),
              CanHandleSwipe(Eq(CastSideSwipeOrigin::BOTTOM)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::BEGIN,
                              Eq(CastSideSwipeOrigin::BOTTOM), drag_point))
      .Times(1);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::CONTINUE,
                              Eq(CastSideSwipeOrigin::BOTTOM), _))
      .Times(1);
  EXPECT_CALL(mock_gesture_handler(),
              HandleSideSwipe(CastSideSwipeEvent::END,
                              Eq(CastSideSwipeOrigin::BOTTOM), end_point))
      .Times(0);

  // Start a drag but don't complete.
  Drag(drag_point, base::Milliseconds(10) /*start_hold_time */,
       base::Milliseconds(1000) /* drag_time */, end_point, 1 /* pointer_id */,
       false /* end_release */);

  // A second drag is started with another finger, but will be ignored as a
  // swipe and all its events eaten.
  Drag(drag_point, base::Milliseconds(10) /*start_hold_time */,
       base::Milliseconds(1000) /* drag_time */, end_point, 2 /* pointer_id */,
       true /* end_release */);

  base::RunLoop().RunUntilIdle();

  // There should be no events generated, even by the second finger.
  EXPECT_EQ(0, test_event_handler().NumTouchEventsReceived());
}

}  // namespace test
}  // namespace chromecast

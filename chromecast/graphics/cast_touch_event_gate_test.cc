// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_touch_event_gate.h"

#include <memory>

#include "base/run_loop.h"
#include "chromecast/graphics/cast_touch_activity_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"

using testing::_;
using testing::AtLeast;
using testing::Eq;

namespace chromecast {

class MockEventHandler : public ui::EventHandler {
 public:
  ~MockEventHandler() override = default;
  MOCK_METHOD1(OnTouchEvent, void(ui::TouchEvent* event));
};

class MockEventGateObserver : public CastTouchActivityObserver {
 public:
  ~MockEventGateObserver() override = default;

  MOCK_METHOD1(OnTouchEventsDisabled, void(bool disabled));
  MOCK_METHOD0(OnTouchActivity, void());
};

class CastTouchEventGateTest : public aura::test::AuraTestBase {
 public:
  ~CastTouchEventGateTest() override = default;

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();

    event_gate_ = std::make_unique<CastTouchEventGate>(root_window());
    event_handler_ = std::make_unique<MockEventHandler>();
    root_window()->AddPreTargetHandler(event_handler_.get());
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window());
    gate_observer_ = std::make_unique<MockEventGateObserver>();
    event_gate_->AddObserver(gate_observer_.get());
  }

  void TearDown() override {
    root_window()->RemovePreTargetHandler(event_handler_.get());
    event_gate_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  CastTouchEventGate& event_gate() { return *event_gate_; }
  MockEventGateObserver& gate_observer() { return *gate_observer_; }
  MockEventHandler& mock_event_handler() { return *event_handler_; }
  ui::test::EventGenerator& event_generator() { return *generator_; }

 private:
  std::unique_ptr<CastTouchEventGate> event_gate_;
  std::unique_ptr<MockEventGateObserver> gate_observer_;
  std::unique_ptr<MockEventHandler> event_handler_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

TEST_F(CastTouchEventGateTest, DisabledByDefaultTest) {
  // Tap leads to 2 touch events.
  EXPECT_CALL(mock_event_handler(), OnTouchEvent(_)).Times(2);

  // Expect no notifications to the observer.
  EXPECT_CALL(gate_observer(), OnTouchEventsDisabled(_)).Times(0);
  EXPECT_CALL(gate_observer(), OnTouchActivity()).Times(0);
  event_generator().GestureTapAt(gfx::Point(50, 50));

  base::RunLoop().RunUntilIdle();
}

TEST_F(CastTouchEventGateTest, EnabledBlocksEvent) {
  // No event should get through.
  EXPECT_CALL(mock_event_handler(), OnTouchEvent(_)).Times(0);

  // We should receive a notification at the observer that the gate was enabled.
  EXPECT_CALL(gate_observer(), OnTouchEventsDisabled(Eq(true))).Times(1);

  // Also that it was disabled at destruction.
  EXPECT_CALL(gate_observer(), OnTouchEventsDisabled(Eq(false))).Times(1);

  // And that the tap event was observed (multiple events)
  EXPECT_CALL(gate_observer(), OnTouchActivity()).Times(AtLeast(2));

  event_gate().SetEnabled(true);
  event_generator().GestureTapAt(gfx::Point(50, 50));

  base::RunLoop().RunUntilIdle();
}

TEST_F(CastTouchEventGateTest, EnableThenDisable) {
  EXPECT_CALL(mock_event_handler(), OnTouchEvent(_)).Times(2);
  EXPECT_CALL(gate_observer(), OnTouchEventsDisabled(Eq(true))).Times(1);
  EXPECT_CALL(gate_observer(), OnTouchEventsDisabled(Eq(false))).Times(1);
  EXPECT_CALL(gate_observer(), OnTouchActivity()).Times(AtLeast(2));

  event_gate().SetEnabled(true);
  event_generator().GestureTapAt(gfx::Point(50, 50));
  event_gate().SetEnabled(false);
  event_generator().GestureTapAt(gfx::Point(50, 50));

  base::RunLoop().RunUntilIdle();
}

}  // namespace chromecast

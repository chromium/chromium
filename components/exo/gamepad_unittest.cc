// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gamepad.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {

namespace {

class MockGamepadObserver : public GamepadObserver {
 public:
  MockGamepadObserver() = default;
  // Overridden from GamepadObserver:
  MOCK_METHOD(void, OnGamepadDestroying, (Gamepad * gamepad), (override));
};

class MockGamepadDelegate : public GamepadDelegate {
 public:
  MockGamepadDelegate() = default;

  // Overridden from GamepadDelegate:
  MOCK_METHOD(void, OnRemoved, (), (override));
  MOCK_METHOD(void,
              OnAxis,
              (int axis, double value, base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void,
              OnButton,
              (int button, bool pressed, base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void, OnFrame, (base::TimeTicks timestamp), (override));
};

class GamepadTest : public testing::Test {
 public:
  GamepadTest() {
    ui::GamepadDevice device(
        ui::InputDevice(0, ui::InputDeviceType::INPUT_DEVICE_USB, "gamepad"),
        std::vector<ui::GamepadDevice::Axis>(), true);
    gamepad_ = std::make_unique<Gamepad>(device);
  }

  GamepadTest(const GamepadTest&) = delete;
  GamepadTest& operator=(const GamepadTest&) = delete;

  std::unique_ptr<Gamepad> gamepad_;
};

TEST_F(GamepadTest, GamepadObserverTest) {
  MockGamepadObserver observer1;
  MockGamepadObserver observer2;

  gamepad_->AddObserver(&observer1);
  gamepad_->AddObserver(&observer2);
  EXPECT_TRUE(gamepad_->HasObserver(&observer1));
  EXPECT_TRUE(gamepad_->HasObserver(&observer2));

  gamepad_->RemoveObserver(&observer1);
  EXPECT_FALSE(gamepad_->HasObserver(&observer1));
  EXPECT_TRUE(gamepad_->HasObserver(&observer2));

  EXPECT_CALL(observer1, OnGamepadDestroying(gamepad_.get())).Times(0);
  EXPECT_CALL(observer2, OnGamepadDestroying(gamepad_.get()));
  gamepad_.reset();
}

TEST_F(GamepadTest, GamepadDelegateTest) {
  auto delegate = std::make_unique<MockGamepadDelegate>();
  EXPECT_CALL(*delegate, OnRemoved()).Times(1);

  gamepad_->SetDelegate(std::move(delegate));

  gamepad_.reset();
}

TEST_F(GamepadTest, OnGamepadEventTest) {
  constexpr int gamepad_id = 0;
  constexpr uint16_t code = 310;
  constexpr double value = 1;
  base::TimeTicks expected_time = base::TimeTicks::Now();

  auto delegate = std::make_unique<MockGamepadDelegate>();
  EXPECT_CALL(*delegate, OnButton(code, value, expected_time)).Times(1);
  EXPECT_CALL(*delegate, OnAxis(code, value, expected_time)).Times(1);
  EXPECT_CALL(*delegate, OnFrame(expected_time)).Times(1);
  EXPECT_CALL(*delegate, OnRemoved()).Times(1);

  gamepad_->SetDelegate(std::move(delegate));

  gamepad_->OnGamepadEvent(ui::GamepadEvent(
      gamepad_id, ui::GamepadEventType::BUTTON, code, value, expected_time));
  gamepad_->OnGamepadEvent(ui::GamepadEvent(
      gamepad_id, ui::GamepadEventType::AXIS, code, value, expected_time));
  gamepad_->OnGamepadEvent(ui::GamepadEvent(
      gamepad_id, ui::GamepadEventType::FRAME, code, value, expected_time));

  gamepad_.reset();
}

TEST_F(GamepadTest, GamepadDestroyedTest) {
  MockGamepadObserver observer1;
  MockGamepadObserver observer2;
  gamepad_->AddObserver(&observer1);
  gamepad_->AddObserver(&observer2);
  EXPECT_TRUE(gamepad_->HasObserver(&observer1));
  EXPECT_TRUE(gamepad_->HasObserver(&observer2));

  auto delegate = std::make_unique<MockGamepadDelegate>();
  EXPECT_CALL(*delegate, OnRemoved()).Times(1);

  gamepad_->SetDelegate(std::move(delegate));

  EXPECT_CALL(observer1, OnGamepadDestroying(gamepad_.get())).Times(1);
  EXPECT_CALL(observer2, OnGamepadDestroying(gamepad_.get())).Times(1);
  gamepad_.reset();
}
}  // namespace
}  // namespace exo

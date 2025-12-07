// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gamepad.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {

namespace {

constexpr int64_t kDurationMillis = 0x8000;
constexpr base::TimeDelta kPendingTaskDuration =
    base::Milliseconds(kDurationMillis);
constexpr base::TimeDelta kPendingMaxTaskDuration =
    base::Milliseconds(kMaxDurationMillis);
constexpr uint8_t kAmplitude = 128;

class TestGamepad : public Gamepad {
 public:
  TestGamepad(const ui::GamepadDevice& device)
      : Gamepad(device),
        send_vibrate_count_(0),
        send_cancel_vibration_count_(0) {}

  TestGamepad(const TestGamepad&) = delete;
  TestGamepad& operator=(const TestGamepad& other) = delete;

  void SendVibrate(uint8_t amplitude, int64_t duration_millis) override {
    send_vibrate_count_++;
    last_vibrate_amplitude_ = amplitude;
    last_vibrate_duration_ = duration_millis;
  }

  void SendCancelVibration() override { send_cancel_vibration_count_++; }

  uint8_t last_vibrate_amplitude_;
  int64_t last_vibrate_duration_;
  int send_vibrate_count_;
  int send_cancel_vibration_count_;
};

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
    gamepad_ = std::make_unique<TestGamepad>(device);
  }

  GamepadTest(const GamepadTest&) = delete;
  GamepadTest& operator=(const GamepadTest&) = delete;

  void SetUp() override {
    testing::Test::SetUp();
    // Allow test to signal to gamepad that it can vibrate.
    scoped_feature_list_.InitAndEnableFeature(ash::features::kGamepadVibration);
    gamepad_->OnGamepadFocused();
  }

  std::unique_ptr<TestGamepad> gamepad_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GamepadTest, OneShotVibrationTest) {
  EXPECT_EQ(0, gamepad_->send_vibrate_count_);
  EXPECT_EQ(0, gamepad_->send_cancel_vibration_count_);

  gamepad_->Vibrate({kDurationMillis}, {kAmplitude}, -1);
  task_environment_.FastForwardBy(base::Milliseconds(kDurationMillis / 2));
  EXPECT_EQ(1, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kDurationMillis, gamepad_->last_vibrate_duration_);

  // Cancel vibration when it's halfway through.
  gamepad_->CancelVibration();
  EXPECT_EQ(1, gamepad_->send_cancel_vibration_count_);
}

TEST_F(GamepadTest, OneShotVibrationTooLongTest) {
  EXPECT_EQ(0, gamepad_->send_vibrate_count_);

  gamepad_->Vibrate({kMaxDurationMillis * 3}, {kAmplitude}, -1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kMaxDurationMillis, gamepad_->last_vibrate_duration_);

  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());
  task_environment_.FastForwardBy(kPendingMaxTaskDuration);
  EXPECT_EQ(2, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kMaxDurationMillis, gamepad_->last_vibrate_duration_);

  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());
  task_environment_.FastForwardBy(kPendingMaxTaskDuration);
  EXPECT_EQ(3, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kMaxDurationMillis, gamepad_->last_vibrate_duration_);

  // Complete the last vibration and make sure no more vibration is scheduled.
  task_environment_.FastForwardBy(kPendingMaxTaskDuration);
  EXPECT_FALSE(task_environment_.NextTaskIsDelayed());
}

TEST_F(GamepadTest, WaveformVibrationTest) {
  EXPECT_EQ(0, gamepad_->send_vibrate_count_);

  gamepad_->Vibrate({kDurationMillis, kDurationMillis}, {kAmplitude, 0}, -1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kDurationMillis, gamepad_->last_vibrate_duration_);

  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());
  task_environment_.FastForwardBy(kPendingTaskDuration);
  EXPECT_EQ(2, gamepad_->send_vibrate_count_);
  EXPECT_EQ(0, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kDurationMillis, gamepad_->last_vibrate_duration_);

  // Complete the last vibration and make sure no more vibration is scheduled.
  task_environment_.FastForwardBy(kPendingTaskDuration);
  EXPECT_FALSE(task_environment_.NextTaskIsDelayed());
}

TEST_F(GamepadTest, VibrationWithRepeatTest) {
  EXPECT_EQ(0, gamepad_->send_vibrate_count_);
  EXPECT_EQ(0, gamepad_->send_cancel_vibration_count_);

  gamepad_->Vibrate({kMaxDurationMillis, kDurationMillis}, {kAmplitude, 0}, 0);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kMaxDurationMillis, gamepad_->last_vibrate_duration_);

  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());
  task_environment_.FastForwardBy(kPendingMaxTaskDuration);
  EXPECT_EQ(2, gamepad_->send_vibrate_count_);
  EXPECT_EQ(0, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kDurationMillis, gamepad_->last_vibrate_duration_);

  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());
  task_environment_.FastForwardBy(kPendingTaskDuration);
  EXPECT_EQ(3, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kMaxDurationMillis, gamepad_->last_vibrate_duration_);

  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());
  task_environment_.FastForwardBy(kPendingTaskDuration);
  gamepad_->CancelVibration();

  EXPECT_EQ(1, gamepad_->send_cancel_vibration_count_);
  EXPECT_EQ(3, gamepad_->send_vibrate_count_);
}

TEST_F(GamepadTest, OverrideVibrationTest) {
  EXPECT_EQ(0, gamepad_->send_vibrate_count_);

  gamepad_->Vibrate({kDurationMillis, kDurationMillis}, {kAmplitude, 0}, -1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kDurationMillis, gamepad_->last_vibrate_duration_);

  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());
  task_environment_.FastForwardBy(kPendingTaskDuration / 2);

  // At this point, we're halfway through the first OneShot vibration in the
  // duration vector.
  gamepad_->Vibrate({kMaxDurationMillis}, {kAmplitude / 2}, -1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude / 2, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kMaxDurationMillis, gamepad_->last_vibrate_duration_);

  // Make sure that the remaining vibration from the first call is no longer in
  // the queue.
  task_environment_.FastForwardBy(kPendingMaxTaskDuration);
  EXPECT_FALSE(task_environment_.NextTaskIsDelayed());
  // Verify that no extra vibration calls were made.
  EXPECT_EQ(2, gamepad_->send_vibrate_count_);
}

TEST_F(GamepadTest, NoFocusTest) {
  EXPECT_EQ(0, gamepad_->send_vibrate_count_);
  gamepad_->OnGamepadFocusLost();

  gamepad_->Vibrate({kDurationMillis}, {kAmplitude}, -1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, gamepad_->send_vibrate_count_);
}

TEST_F(GamepadTest, FocusLostTest) {
  EXPECT_EQ(0, gamepad_->send_vibrate_count_);
  EXPECT_EQ(0, gamepad_->send_cancel_vibration_count_);

  gamepad_->Vibrate({kDurationMillis, kDurationMillis}, {kAmplitude, 0}, -1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kDurationMillis, gamepad_->last_vibrate_duration_);

  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());
  gamepad_->OnGamepadFocusLost();
  task_environment_.FastForwardBy(kPendingTaskDuration);

  // When focus is lost, CancelVibration is sent, and no more vibration can be
  // scheduled.
  EXPECT_EQ(1, gamepad_->send_cancel_vibration_count_);
  EXPECT_EQ(1, gamepad_->send_vibrate_count_);

  // While focus is not regained, gamepad cannot vibrate.
  gamepad_->Vibrate({kDurationMillis}, {kAmplitude}, -1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, gamepad_->send_vibrate_count_);

  // If focus is regained, gamepad can vibrate again.
  gamepad_->OnGamepadFocused();
  gamepad_->Vibrate({kDurationMillis / 2}, {kAmplitude / 2}, -1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, gamepad_->send_vibrate_count_);
  EXPECT_EQ(kAmplitude / 2, gamepad_->last_vibrate_amplitude_);
  EXPECT_EQ(kDurationMillis / 2, gamepad_->last_vibrate_duration_);
}

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

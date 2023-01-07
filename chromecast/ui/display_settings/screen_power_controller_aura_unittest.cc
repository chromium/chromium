// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings/screen_power_controller_aura.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromecast/ui/display_settings/screen_power_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace test {
namespace {

using ::testing::_;

// These constants should be the same as screen_power_controller_aura.cc.
constexpr base::TimeDelta kScreenOnOffDuration = base::Milliseconds(200);
constexpr base::TimeDelta kDisplayPowerOnDelay = base::Milliseconds(35);
constexpr base::TimeDelta kDisplayPowerOffDelay = base::Milliseconds(85);

class MockScreenPowerControllerDelegate
    : public ScreenPowerController::Delegate {
 public:
  ~MockScreenPowerControllerDelegate() override = default;

  // ScreenPowerController::Delegate implementation:
  MOCK_METHOD(void, SetScreenPowerOn, (PowerToggleCallback callback), (override));
  MOCK_METHOD(void, SetScreenPowerOff, (PowerToggleCallback callback), (override));
  MOCK_METHOD(void,
              SetScreenBrightnessOn,
              (bool brightness_on, base::TimeDelta duration),
              (override));
};

class ScreenPowerControllerAuraTest : public ::testing::Test {
 public:
  ScreenPowerControllerAuraTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        delegate_(std::make_unique<MockScreenPowerControllerDelegate>()),
        screen_power_controller_(
            ScreenPowerController::Create(delegate_.get())) {}

 protected:
  void BrightnessOff() {
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(false, kScreenOnOffDuration));
    screen_power_controller_->SetScreenOff();
  }

  void PowerOff() {
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(false, kScreenOnOffDuration));
    EXPECT_CALL(*delegate_, SetScreenPowerOff(_))
            .WillOnce(base::test::RunOnceCallback<0>(true));
    screen_power_controller_->SetAllowScreenPowerOff(true);
    screen_power_controller_->SetScreenOff();
    task_env_.FastForwardBy(kDisplayPowerOffDelay + kScreenOnOffDuration);
  }

  base::test::TaskEnvironment task_env_;
  std::unique_ptr<MockScreenPowerControllerDelegate> delegate_;
  std::unique_ptr<ScreenPowerController> screen_power_controller_;
};

}  // namespace

TEST_F(ScreenPowerControllerAuraTest, PowerOnToBrightnessOff) {
  EXPECT_CALL(*delegate_, SetScreenBrightnessOn(false, kScreenOnOffDuration));
  EXPECT_CALL(*delegate_, SetScreenPowerOn(_)).Times(0);
  EXPECT_CALL(*delegate_, SetScreenPowerOff(_)).Times(0);
  screen_power_controller_->SetScreenOff();
}

TEST_F(ScreenPowerControllerAuraTest, PowerOnToPowerOff) {
  {
    ::testing::InSequence s;
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(false, kScreenOnOffDuration));
    EXPECT_CALL(*delegate_, SetScreenPowerOn(_)).Times(0);
    EXPECT_CALL(*delegate_, SetScreenPowerOff(_))
        .WillOnce(base::test::RunOnceCallback<0>(true));
    screen_power_controller_->SetAllowScreenPowerOff(true);
    screen_power_controller_->SetScreenOff();
    task_env_.FastForwardBy(kDisplayPowerOffDelay + kScreenOnOffDuration);
  }
}

TEST_F(ScreenPowerControllerAuraTest, BrightnessOffToPowerOff) {
  BrightnessOff();

  EXPECT_CALL(*delegate_, SetScreenBrightnessOn(_, _)).Times(0);
  EXPECT_CALL(*delegate_, SetScreenPowerOn(_)).Times(0);
  EXPECT_CALL(*delegate_, SetScreenPowerOff(_))
      .WillOnce(base::test::RunOnceCallback<0>(true));
  screen_power_controller_->SetAllowScreenPowerOff(true);
  task_env_.FastForwardBy(kDisplayPowerOffDelay + kScreenOnOffDuration);
}

TEST_F(ScreenPowerControllerAuraTest, BrightnessOffToBrightnessOn) {
  BrightnessOff();

  EXPECT_CALL(*delegate_, SetScreenBrightnessOn(true, kScreenOnOffDuration))
      .Times(1);
  EXPECT_CALL(*delegate_, SetScreenPowerOn(_)).Times(0);
  EXPECT_CALL(*delegate_, SetScreenPowerOff(_)).Times(0);
  screen_power_controller_->SetScreenOn();
}

TEST_F(ScreenPowerControllerAuraTest, PowerOffToBrightnessOn) {
  PowerOff();

  {
    ::testing::InSequence s;
    EXPECT_CALL(*delegate_, SetScreenPowerOn(_))
        .WillOnce(base::test::RunOnceCallback<0>(true));
    EXPECT_CALL(*delegate_, SetScreenPowerOff(_)).Times(0);
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(true, kScreenOnOffDuration))
        .Times(1);
    screen_power_controller_->SetScreenOn();
    task_env_.FastForwardBy(kDisplayPowerOnDelay);
  }
}

TEST_F(ScreenPowerControllerAuraTest, PowerOffToBrightnessOff) {
  PowerOff();

  EXPECT_CALL(*delegate_, SetScreenPowerOn(_))
      .WillOnce(base::test::RunOnceCallback<0>(true));
  EXPECT_CALL(*delegate_, SetScreenPowerOff(_)).Times(0);
  EXPECT_CALL(*delegate_, SetScreenBrightnessOn(_, _)).Times(0);
  screen_power_controller_->SetAllowScreenPowerOff(false);
  task_env_.FastForwardBy(kDisplayPowerOnDelay);
}

TEST_F(ScreenPowerControllerAuraTest, PowerOnToPowerOffToPowerOn) {
  {
    ::testing::InSequence s;
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(false, kScreenOnOffDuration));
    EXPECT_CALL(*delegate_, SetScreenPowerOff(_)).Times(0);
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(true, kScreenOnOffDuration));
    EXPECT_CALL(*delegate_, SetScreenPowerOn(_)).Times(0);
    screen_power_controller_->SetAllowScreenPowerOff(true);
    screen_power_controller_->SetScreenOff();
    screen_power_controller_->SetScreenOn();
    task_env_.FastForwardBy(kDisplayPowerOffDelay + kScreenOnOffDuration +
                            kDisplayPowerOnDelay);
  }
}

TEST_F(ScreenPowerControllerAuraTest, PowerOnToPowerOffToBrightnessOff) {
  {
    ::testing::InSequence s;
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(false, kScreenOnOffDuration));
    EXPECT_CALL(*delegate_, SetScreenPowerOff(_)).Times(0);
    EXPECT_CALL(*delegate_, SetScreenPowerOn(_)).Times(0);
    screen_power_controller_->SetAllowScreenPowerOff(true);
    screen_power_controller_->SetScreenOff();
    screen_power_controller_->SetAllowScreenPowerOff(false);
    task_env_.FastForwardBy(kDisplayPowerOffDelay + kScreenOnOffDuration +
                            kDisplayPowerOnDelay);
  }
}

TEST_F(ScreenPowerControllerAuraTest, PowerOffToPowerOnToPowerOff) {
  PowerOff();

  {
    ::testing::InSequence s;
    EXPECT_CALL(*delegate_, SetScreenPowerOn(_))
        .WillOnce(base::test::RunOnceCallback<0>(true));
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(_, _)).Times(0);
    EXPECT_CALL(*delegate_, SetScreenPowerOff(_))
        .WillOnce(base::test::RunOnceCallback<0>(true));
    screen_power_controller_->SetScreenOn();
    screen_power_controller_->SetScreenOff();
    task_env_.FastForwardBy(kDisplayPowerOffDelay + kScreenOnOffDuration +
                            kDisplayPowerOnDelay);
  }
}

TEST_F(ScreenPowerControllerAuraTest, DoubleTriggeredPowerOff) {
  screen_power_controller_->SetAllowScreenPowerOff(true);

  {
    ::testing::InSequence s;
    EXPECT_CALL(*delegate_, SetScreenPowerOn(_)).Times(0);
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(false, kScreenOnOffDuration));
    EXPECT_CALL(*delegate_, SetScreenPowerOff(_))
        .WillOnce(base::test::RunOnceCallback<0>(true));
    screen_power_controller_->SetScreenOff();
    screen_power_controller_->SetScreenOff();
    task_env_.FastForwardBy(kDisplayPowerOffDelay + kScreenOnOffDuration +
                            kDisplayPowerOnDelay);
  }
}

TEST_F(ScreenPowerControllerAuraTest, DoubleTriggeredPowerOn) {
  PowerOff();

  {
    ::testing::InSequence s;
    EXPECT_CALL(*delegate_, SetScreenPowerOff(_)).Times(0);
    EXPECT_CALL(*delegate_, SetScreenPowerOn(_))
            .WillOnce(base::test::RunOnceCallback<0>(true));
    EXPECT_CALL(*delegate_, SetScreenBrightnessOn(true, kScreenOnOffDuration));
    screen_power_controller_->SetScreenOn();
    screen_power_controller_->SetScreenOn();
    task_env_.FastForwardBy(kDisplayPowerOffDelay + kScreenOnOffDuration +
                            kDisplayPowerOnDelay);
  }
}

}  // namespace test
}  // namespace chromecast

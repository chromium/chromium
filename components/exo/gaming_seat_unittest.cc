// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gaming_seat.h"

#include <memory>
#include <vector>

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/exo/buffer.h"
#include "components/exo/gamepad.h"
#include "components/exo/gamepad_delegate.h"
#include "components/exo/gaming_seat_delegate.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"

namespace exo {
namespace {

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

class MockGamingSeatDelegate : public GamingSeatDelegate {
 public:
  MOCK_METHOD(bool,
              CanAcceptGamepadEventsForSurface,
              (Surface * surface),
              (const, override));
  MOCK_METHOD(void, GamepadAdded, (Gamepad & gamepad), (override));
  MOCK_METHOD(void, Die, (), ());
  void OnGamingSeatDestroying(GamingSeat*) override { delete this; }
  ~MockGamingSeatDelegate() override { Die(); }
};

class GamingSeatTest : public test::ExoTestBase {
 public:
  GamingSeatTest() = default;

  GamingSeatTest(const GamingSeatTest&) = delete;
  GamingSeatTest& operator=(const GamingSeatTest&) = delete;

  void InitializeGamingSeat(MockGamingSeatDelegate* delegate) {
    gaming_seat_ = std::make_unique<GamingSeat>(delegate);
  }

  void DestroyGamingSeat(MockGamingSeatDelegate* delegate) {
    EXPECT_CALL(*delegate, Die()).Times(1);
    gaming_seat_.reset();
  }

  void UpdateGamepadDevice(const std::vector<int>& gamepad_device_ids) {
    std::vector<ui::GamepadDevice> gamepad_devices;
    for (auto& id : gamepad_device_ids) {
      gamepad_devices.emplace_back(
          ui::InputDevice(id, ui::InputDeviceType::INPUT_DEVICE_USB, "gamepad"),
          std::vector<ui::GamepadDevice::Axis>(),
          /*supports_vibration_rumble=*/false);
    }
    ui::GamepadProviderOzone::GetInstance()->DispatchGamepadDevicesUpdated(
        gamepad_devices);
  }

  void SendFrameToGamepads(const std::vector<int>& gamepad_device_ids) {
    for (auto& id : gamepad_device_ids) {
      ui::GamepadEvent event(id, ui::GamepadEventType::FRAME, 0, 0,
                             base::TimeTicks());
      ui::GamepadProviderOzone::GetInstance()->DispatchGamepadEvent(event);
    }
  }

  void SendButtonToGamepads(const std::vector<int>& gamepad_device_ids,
                            base::TimeTicks timestamp) {
    for (auto& id : gamepad_device_ids) {
      ui::GamepadEvent event(id, ui::GamepadEventType::BUTTON, 310, 1,
                             timestamp);
      ui::GamepadProviderOzone::GetInstance()->DispatchGamepadEvent(event);
    }
  }

 protected:
  std::unique_ptr<GamingSeat> gaming_seat_;
};

TEST_F(GamingSeatTest, ConnectionChange) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamingSeatDelegate>* gaming_seat_delegate =
      new testing::StrictMock<MockGamingSeatDelegate>();
  EXPECT_CALL(*gaming_seat_delegate,
              CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamingSeat(gaming_seat_delegate);
  std::unique_ptr<MockGamepadDelegate> gamepad_delegates[6];
  for (auto& delegate : gamepad_delegates)
    delegate = std::make_unique<testing::StrictMock<MockGamepadDelegate>>();

  {  // Test sequence
    testing::InSequence s;
    // Connect 2 gamepads.
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Invoke([&gamepad_delegates](auto& gamepad) {
          gamepad.SetDelegate(std::move(gamepad_delegates[0]));
        }));
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Invoke([&gamepad_delegates](auto& gamepad) {
          gamepad.SetDelegate(std::move(gamepad_delegates[1]));
        }));
    // Send frame to connected gamepad.
    EXPECT_CALL(*gamepad_delegates[0], OnFrame(testing::_)).Times(1);
    EXPECT_CALL(*gamepad_delegates[1], OnFrame(testing::_)).Times(1);
    // Connect 3 more.
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Invoke([&gamepad_delegates](auto& gamepad) {
          gamepad.SetDelegate(std::move(gamepad_delegates[2]));
        }));
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Invoke([&gamepad_delegates](auto& gamepad) {
          gamepad.SetDelegate(std::move(gamepad_delegates[3]));
        }));
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Invoke([&gamepad_delegates](auto& gamepad) {
          gamepad.SetDelegate(std::move(gamepad_delegates[4]));
        }));
    // Send frame to all gamepads.
    EXPECT_CALL(*gamepad_delegates[0], OnFrame(testing::_)).Times(1);
    EXPECT_CALL(*gamepad_delegates[1], OnFrame(testing::_)).Times(1);
    EXPECT_CALL(*gamepad_delegates[2], OnFrame(testing::_)).Times(1);
    EXPECT_CALL(*gamepad_delegates[3], OnFrame(testing::_)).Times(1);
    EXPECT_CALL(*gamepad_delegates[4], OnFrame(testing::_)).Times(1);
    // Disconnect gamepads 0, 2 and 4.
    EXPECT_CALL(*gamepad_delegates[0], OnRemoved()).Times(1);
    EXPECT_CALL(*gamepad_delegates[2], OnRemoved()).Times(1);
    EXPECT_CALL(*gamepad_delegates[4], OnRemoved()).Times(1);
    // Connect a new gamepad.
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Invoke([&gamepad_delegates](auto& gamepad) {
          gamepad.SetDelegate(std::move(gamepad_delegates[5]));
        }));
    // Send frame to all gamepads.
    EXPECT_CALL(*gamepad_delegates[1], OnFrame(testing::_)).Times(1);
    EXPECT_CALL(*gamepad_delegates[3], OnFrame(testing::_)).Times(1);
    EXPECT_CALL(*gamepad_delegates[5], OnFrame(testing::_)).Times(1);
  }

  // The rest of gamepads should be disconnected after GamingSeat is
  // destroyed.
  EXPECT_CALL(*gamepad_delegates[1], OnRemoved()).Times(1);
  EXPECT_CALL(*gamepad_delegates[3], OnRemoved()).Times(1);
  EXPECT_CALL(*gamepad_delegates[5], OnRemoved()).Times(1);

  // Gamepad connected.
  UpdateGamepadDevice({0, 1});
  SendFrameToGamepads({0, 1});
  UpdateGamepadDevice({0, 1, 2, 3, 4});
  SendFrameToGamepads({0, 1, 2, 3, 4});
  UpdateGamepadDevice({1, 2, 3, 4});
  UpdateGamepadDevice({1, 3, 4});
  UpdateGamepadDevice({1, 3});
  UpdateGamepadDevice({1, 3, 5});
  SendFrameToGamepads({1, 2, 3, 4, 5});
  DestroyGamingSeat(gaming_seat_delegate);
  UpdateGamepadDevice({});
}

TEST_F(GamingSeatTest, Timestamp) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamingSeatDelegate>* gaming_seat_delegate =
      new testing::StrictMock<MockGamingSeatDelegate>();
  EXPECT_CALL(*gaming_seat_delegate,
              CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamingSeat(gaming_seat_delegate);
  auto gamepad_delegate =
      std::make_unique<testing::StrictMock<MockGamepadDelegate>>();
  base::TimeTicks expected_time = base::TimeTicks::Now();

  {  // Test sequence
    testing::InSequence s;

    // Connect gamepad.
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Invoke([&gamepad_delegate](auto& gamepad) {
          gamepad.SetDelegate(std::move(gamepad_delegate));
        }));
    // Send button to connected gamepad. Expect correct timestamp.
    EXPECT_CALL(*gamepad_delegate,
                OnButton(testing::_, testing::_, testing::Eq(expected_time)))
        .Times(1);
  }

  // Disconnect gamepad.
  EXPECT_CALL(*gamepad_delegate, OnRemoved()).Times(1);

  // Gamepad connected.
  UpdateGamepadDevice({1});
  SendButtonToGamepads({1}, expected_time);
  UpdateGamepadDevice({});
  DestroyGamingSeat(gaming_seat_delegate);
}

}  // namespace
}  // namespace exo

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/gaming_seat.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "components/exo/buffer.h"
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

class MockGamingSeatDelegate : public GamingSeatDelegate {
 public:
  MOCK_CONST_METHOD1(CanAcceptGamepadEventsForSurface, bool(Surface*));
  MOCK_METHOD1(GamepadAdded, GamepadDelegate*(const ui::GamepadDevice&));
  MOCK_METHOD0(Die, void());
  void OnGamingSeatDestroying(GamingSeat*) override { delete this; }
  ~MockGamingSeatDelegate() { Die(); }
};

class MockGamepadDelegate : public GamepadDelegate {
 public:
  MockGamepadDelegate() {}

  // Overridden from GamepadDelegate:
  MOCK_METHOD0(OnRemoved, void());
  MOCK_METHOD2(OnAxis, void(int, double));
  MOCK_METHOD2(OnButton, void(int, bool));
  MOCK_METHOD0(OnFrame, void());
};

class GamingSeatTest : public test::ExoTestBase {
 public:
  GamingSeatTest() {}
  void InitializeGamingSeat(MockGamingSeatDelegate* delegate) {
    gaming_seat_.reset(new GamingSeat(delegate));
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
          std::vector<ui::GamepadDevice::Axis>());
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

 protected:
  std::unique_ptr<GamingSeat> gaming_seat_;

  DISALLOW_COPY_AND_ASSIGN(GamingSeatTest);
};

TEST_F(GamingSeatTest, ConnectionChange) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  testing::StrictMock<MockGamingSeatDelegate>* gaming_seat_delegate =
      new testing::StrictMock<MockGamingSeatDelegate>();
  EXPECT_CALL(*gaming_seat_delegate,
              CanAcceptGamepadEventsForSurface(testing::_))
      .WillOnce(testing::Return(true));

  InitializeGamingSeat(gaming_seat_delegate);
  testing::StrictMock<MockGamepadDelegate> gamepad_delegate[6];

  {  // Test sequence
    testing::InSequence s;
    // Connect 2 gamepads.
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Return(&gamepad_delegate[0]))
        .WillOnce(testing::Return(&gamepad_delegate[1]));
    // Send frame to connected gamepad.
    EXPECT_CALL(gamepad_delegate[0], OnFrame()).Times(1);
    EXPECT_CALL(gamepad_delegate[1], OnFrame()).Times(1);
    // Connect 3 more.
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Return(&gamepad_delegate[2]))
        .WillOnce(testing::Return(&gamepad_delegate[3]))
        .WillOnce(testing::Return(&gamepad_delegate[4]));
    // Send frame to all gamepads.
    EXPECT_CALL(gamepad_delegate[0], OnFrame()).Times(1);
    EXPECT_CALL(gamepad_delegate[1], OnFrame()).Times(1);
    EXPECT_CALL(gamepad_delegate[2], OnFrame()).Times(1);
    EXPECT_CALL(gamepad_delegate[3], OnFrame()).Times(1);
    EXPECT_CALL(gamepad_delegate[4], OnFrame()).Times(1);
    // Disconnect gamepad 0 and gamepad 2 and connect a new gamepad.
    EXPECT_CALL(gamepad_delegate[0], OnRemoved()).Times(1);
    EXPECT_CALL(gamepad_delegate[2], OnRemoved()).Times(1);
    EXPECT_CALL(gamepad_delegate[4], OnRemoved()).Times(1);
    EXPECT_CALL(*gaming_seat_delegate, GamepadAdded(testing::_))
        .WillOnce(testing::Return(&gamepad_delegate[5]));
    // Send frame to all gamepads.
    EXPECT_CALL(gamepad_delegate[1], OnFrame()).Times(1);
    EXPECT_CALL(gamepad_delegate[3], OnFrame()).Times(1);
    EXPECT_CALL(gamepad_delegate[5], OnFrame()).Times(1);

    // disconnect other gamepads
    EXPECT_CALL(gamepad_delegate[1], OnRemoved()).Times(1);
    EXPECT_CALL(gamepad_delegate[3], OnRemoved()).Times(1);
    EXPECT_CALL(gamepad_delegate[5], OnRemoved()).Times(1);
  }
  // Gamepad connected.
  UpdateGamepadDevice({0, 1});
  SendFrameToGamepads({0, 1});
  UpdateGamepadDevice({0, 1, 2, 3, 4});
  SendFrameToGamepads({0, 1, 2, 3, 4});
  UpdateGamepadDevice({1, 3, 5});
  SendFrameToGamepads({1, 2, 3, 4, 5});
  DestroyGamingSeat(gaming_seat_delegate);
}

}  // namespace
}  // namespace exo

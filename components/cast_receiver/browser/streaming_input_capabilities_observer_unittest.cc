// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_input_capabilities_observer.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace cast_receiver {

class StreamingInputCapabilitiesObserverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!ui::DeviceDataManager::HasInstance()) {
      ui::DeviceDataManager::CreateInstance();
      created_data_manager_ = true;
    }
  }

  void TearDown() override {
    ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
    ui::DeviceDataManagerTestApi().SetMouseDevices({});
    ui::DeviceDataManagerTestApi().SetTouchscreenDevices({});
    if (created_data_manager_) {
      ui::DeviceDataManager::DeleteInstance();
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  bool created_data_manager_ = false;
};

TEST_F(StreamingInputCapabilitiesObserverTest, EmptyInitially) {
  int callback_count = 0;
  auto* manager = ui::DeviceDataManager::GetInstance();
  StreamingInputCapabilitiesObserver observer(
      manager, base::BindLambdaForTesting(
                   [&](const cast_receiver::InputCapabilities& caps) {
                     EXPECT_EQ(caps.devices_size(), 0);
                     callback_count++;
                   }));

  // Since lists are not complete and we didn't trigger them, no initial update.
  static_cast<ui::DeviceHotplugEventObserver*>(manager)
      ->OnDeviceListsComplete();

  EXPECT_EQ(callback_count, 1);
}

TEST_F(StreamingInputCapabilitiesObserverTest, TriggersOnDeviceListsComplete) {
  auto* manager = ui::DeviceDataManager::GetInstance();

  // Inject a USB keyboard, a virtual keyboard, and a touchscreen.
  std::vector<ui::KeyboardDevice> keyboards;
  keyboards.emplace_back(1, ui::INPUT_DEVICE_USB, "Test Keyboard", "phys_kbd",
                         base::FilePath(), 0x1111, 0x2222, 0);
  keyboards.emplace_back(4, ui::INPUT_DEVICE_UNKNOWN, "Virtual Keyboard",
                         "phys_v_kbd", base::FilePath(), 0, 0, 0);
  static_cast<ui::DeviceHotplugEventObserver*>(manager)
      ->OnKeyboardDevicesUpdated(keyboards);

  std::vector<ui::TouchscreenDevice> touchscreens;
  touchscreens.emplace_back(2, ui::INPUT_DEVICE_INTERNAL, "Test Touch",
                            gfx::Size(1920, 1080), 10);
  static_cast<ui::DeviceHotplugEventObserver*>(manager)
      ->OnTouchscreenDevicesUpdated(touchscreens);

  int callback_count = 0;
  cast_receiver::InputCapabilities received_caps;

  StreamingInputCapabilitiesObserver observer(
      manager, base::BindLambdaForTesting(
                   [&](const cast_receiver::InputCapabilities& caps) {
                     received_caps = caps;
                     callback_count++;
                   }));

  // Trigger OnDeviceListsComplete to signal initial scan is done.
  static_cast<ui::DeviceHotplugEventObserver*>(manager)
      ->OnDeviceListsComplete();

  EXPECT_EQ(callback_count, 1);
  ASSERT_EQ(received_caps.devices_size(), 3);

  // Verify USB Keyboard
  EXPECT_EQ(received_caps.devices(0).device_id(), "1");
  EXPECT_EQ(received_caps.devices(0).display_name(), "Test Keyboard");
  EXPECT_EQ(received_caps.devices(0).type(),
            cast_receiver::INPUT_TYPE_KEYBOARD);
  EXPECT_EQ(received_caps.devices(0).vendor_id(), 0x1111);
  EXPECT_EQ(received_caps.devices(0).product_id(), 0x2222);
  EXPECT_FALSE(received_caps.devices(0).keyboard_metadata().is_virtual());

  // Verify Virtual Keyboard
  EXPECT_EQ(received_caps.devices(1).device_id(), "4");
  EXPECT_EQ(received_caps.devices(1).display_name(), "Virtual Keyboard");
  EXPECT_EQ(received_caps.devices(1).type(),
            cast_receiver::INPUT_TYPE_KEYBOARD);
  EXPECT_TRUE(received_caps.devices(1).keyboard_metadata().is_virtual());

  // Verify Touch
  EXPECT_EQ(received_caps.devices(2).device_id(), "2");
  EXPECT_EQ(received_caps.devices(2).display_name(), "Test Touch");
  EXPECT_EQ(received_caps.devices(2).type(), cast_receiver::INPUT_TYPE_TOUCH);
  EXPECT_EQ(received_caps.devices(2).vendor_id(),
            0);  // Default for internal usually
  EXPECT_EQ(received_caps.devices(2).touch_metadata().max_touch_points(), 10);
}

TEST_F(StreamingInputCapabilitiesObserverTest, DeduplicatesIdenticalUpdates) {
  auto* manager = ui::DeviceDataManager::GetInstance();
  static_cast<ui::DeviceHotplugEventObserver*>(manager)
      ->OnDeviceListsComplete();

  int callback_count = 0;
  StreamingInputCapabilitiesObserver observer(
      manager, base::BindLambdaForTesting(
                   [&](const cast_receiver::InputCapabilities& caps) {
                     callback_count++;
                   }));

  // Initial update triggered because lists were already complete.
  EXPECT_EQ(callback_count, 1);

  // Trigger configuration change without actually changing anything.
  static_cast<ui::InputDeviceEventObserver*>(&observer)
      ->OnInputDeviceConfigurationChanged(
          ui::InputDeviceEventObserver::kKeyboard);

  // Callback count should still be 1 because the caps are identical (empty).
  EXPECT_EQ(callback_count, 1);
}

TEST_F(StreamingInputCapabilitiesObserverTest, TriggersOnConfigurationChanged) {
  auto* manager = ui::DeviceDataManager::GetInstance();
  static_cast<ui::DeviceHotplugEventObserver*>(manager)
      ->OnDeviceListsComplete();

  int callback_count = 0;
  cast_receiver::InputCapabilities received_caps;

  StreamingInputCapabilitiesObserver observer(
      manager, base::BindLambdaForTesting(
                   [&](const cast_receiver::InputCapabilities& caps) {
                     received_caps = caps;
                     callback_count++;
                   }));

  EXPECT_EQ(callback_count, 1);
  EXPECT_EQ(received_caps.devices_size(), 0);

  // Hotplug a mouse.
  std::vector<ui::InputDevice> mice;
  mice.emplace_back(3, ui::INPUT_DEVICE_BLUETOOTH, "Test Mouse", "phys_mouse",
                    base::FilePath(), 0x3333, 0x4444, 0);
  static_cast<ui::DeviceHotplugEventObserver*>(manager)->OnMouseDevicesUpdated(
      mice);

  // Notify configuration change for mouse.
  static_cast<ui::InputDeviceEventObserver*>(&observer)
      ->OnInputDeviceConfigurationChanged(ui::InputDeviceEventObserver::kMouse);

  EXPECT_EQ(callback_count, 2);
  ASSERT_EQ(received_caps.devices_size(), 1);
  EXPECT_EQ(received_caps.devices(0).device_id(), "3");
  EXPECT_EQ(received_caps.devices(0).display_name(), "Test Mouse");
  EXPECT_EQ(received_caps.devices(0).type(), cast_receiver::INPUT_TYPE_MOUSE);
  EXPECT_EQ(received_caps.devices(0).vendor_id(), 0x3333);
  EXPECT_EQ(received_caps.devices(0).product_id(), 0x4444);
}

}  // namespace cast_receiver

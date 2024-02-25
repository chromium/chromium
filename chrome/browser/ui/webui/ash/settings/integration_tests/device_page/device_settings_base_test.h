// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_INTEGRATION_TESTS_DEVICE_PAGE_DEVICE_SETTINGS_BASE_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_INTEGRATION_TESTS_DEVICE_PAGE_DEVICE_SETTINGS_BASE_TEST_H_

#include <linux/input-event-codes.h>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

namespace ash {

inline constexpr int kDeviceId1 = 5;
inline constexpr char kPerDeviceKeyboardSubpagePath[] = "per-device-keyboard";

class FakeDeviceManager {
 public:
  FakeDeviceManager();
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager();

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeInternalKeyboard();

 private:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
};

class DeviceSettingsBaseTest : public InteractiveAshTest {
 public:
  DeviceSettingsBaseTest();
  DeviceSettingsBaseTest(const DeviceSettingsBaseTest&) = delete;
  DeviceSettingsBaseTest& operator=(const DeviceSettingsBaseTest&) = delete;
  ~DeviceSettingsBaseTest() override;

  ui::test::InteractiveTestApi::MultiStep LaunchSettingsApp(
      const ui::ElementIdentifier& element_id,
      const std::string& sub_page);
  ui::test::InteractiveTestApi::StepBuilder SetupInternalKeyboard();

  void SetMouseDevices(const std::vector<ui::InputDevice>& mice);
  void SetTouchpadDevices(const std::vector<ui::TouchpadDevice>& touchpads);
  void SetPointingStickDevices(
      const std::vector<ui::InputDevice>& pointing_sticks);

  // Enters lower-case text into the focused html input element.
  ui::test::InteractiveTestApi::StepBuilder EnterLowerCaseText(
      const std::string& text);

  ui::test::InteractiveTestApi::StepBuilder SendKeyPressEvent(
      ui::KeyboardCode key,
      int modifier = ui::EF_NONE);

  // Query to pierce through Shadow DOM to find the keyboard.
  const DeepQuery kKeyboardNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-keyboard",
      "settings-per-device-keyboard-subsection",
      "h2#keyboardName",
  };

  // MixinBasedInProcessBrowserTest:
  void SetUpOnMainThread() override;

 protected:
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
  base::test::ScopedFeatureList feature_list_;
  ui::ElementIdentifier webcontents_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_INTEGRATION_TESTS_DEVICE_PAGE_DEVICE_SETTINGS_BASE_TEST_H_

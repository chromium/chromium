// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

namespace ash {

namespace {

constexpr char kDeviceSectionPath[] = "device";

class DeviceSettingsKeyboardInteractiveUiTest : public InteractiveAshTest {
 public:
  DeviceSettingsKeyboardInteractiveUiTest() {
    feature_list_.InitAndEnableFeature(features::kInputDeviceSettingsSplit);
  }

  // Query to pierce through Shadow DOM to find the touchpad row.
  const DeepQuery kKeyboardRowQuery{
      "os-settings-ui",       "os-settings-main",      "main-page-container",
      "settings-device-page", "#perDeviceKeyboardRow",
  };

  // Query to pierce through Shadow DOM to find the Keyboard header.
  const DeepQuery kKeyboardNameQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-keyboard",
      "settings-per-device-keyboard-subsection",
      "h2#keyboardName",
  };

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();

    // Initialize keyboard.
    std::vector<ui::KeyboardDevice> keyboard_devices;
    keyboard_devices.emplace_back(1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                                  "keyboard");
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(keyboard_devices);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsKeyboardInteractiveUiTest,
                       OpenKeyboardSubpage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);
  RunTestSequence(
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()), Do([&]() {
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            GetActiveUserProfile(), kDeviceSectionPath);
      }),
      WaitForShow(kOsSettingsWebContentsId),
      Log("Waiting for per device section to load"),
      WaitForWebContentsReady(kOsSettingsWebContentsId,
                              chrome::GetOSSettingsUrl(kDeviceSectionPath)),
      WaitForElementExists(kOsSettingsWebContentsId, kKeyboardRowQuery),
      ClickElement(kOsSettingsWebContentsId, kKeyboardRowQuery),
      WaitForElementTextContains(kOsSettingsWebContentsId, kKeyboardNameQuery,
                                 "Built-in Keyboard"));
}

}  // namespace
}  // namespace ash

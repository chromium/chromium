// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {

constexpr char kDeviceSectionPath[] = "device";

class DeviceSettingsSwapPrimaryMouseButtonInteractiveUiTest
    : public InteractiveAshTest {
 public:
  DeviceSettingsSwapPrimaryMouseButtonInteractiveUiTest() {
    feature_list_.InitWithFeatures({features::kInputDeviceSettingsSplit},
                                   {features::kPeripheralCustomization});
  }

  // Query to pierce through Shadow DOM to find the mouse row.
  const DeepQuery kMouseRowQuery{
      "os-settings-ui",       "os-settings-main",   "main-page-container",
      "settings-device-page", "#perDeviceMouseRow",
  };

  const DeepQuery kMouseSwapButtonDropdownQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#mouseSwapButtonDropdown",
      "#dropdownMenu",
  };

  const DeepQuery kCursorAcceleartorToggleQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#mouseAcceleration",
      "#control",
  };

  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();

    // Initialize mouse.
    ui::DeviceDataManagerTestApi().SetMouseDevices(
        {ui::InputDevice(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse")});
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsSwapPrimaryMouseButtonInteractiveUiTest,
                       SwapPrimaryMouseButton) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCursorAccelerationToggleEnabledEvent);
  StateChange cursor_acceleration_toggle_enabled;
  cursor_acceleration_toggle_enabled.type =
      StateChange::Type::kExistsAndConditionTrue;
  cursor_acceleration_toggle_enabled.event =
      kCursorAccelerationToggleEnabledEvent;
  cursor_acceleration_toggle_enabled.where = kCursorAcceleartorToggleQuery;
  cursor_acceleration_toggle_enabled.test_function = "el => !el.disabled";

  RunTestSequence(
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()), Do([&]() {
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            GetActiveUserProfile(), kDeviceSectionPath);
      }),
      WaitForShow(kOsSettingsWebContentsId),
      WaitForWebContentsReady(kOsSettingsWebContentsId,
                              chrome::GetOSSettingsUrl(kDeviceSectionPath)),
      Log("Waiting for per device mouse row to be visible"),
      WaitForElementExists(kOsSettingsWebContentsId, kMouseRowQuery),
      ClickElement(kOsSettingsWebContentsId, kMouseRowQuery),
      Log("Waiting for swap primary mouse toggle to be visible"),
      WaitForElementExists(kOsSettingsWebContentsId,
                           kMouseSwapButtonDropdownQuery),
      Log("Selecting 'Right button' from the dropdown menu"),
      ExecuteJsAt(kOsSettingsWebContentsId, kMouseSwapButtonDropdownQuery,
                  "(el) => {el.selectedIndex = 1; el.dispatchEvent(new "
                  "Event('change'));}"),
      Log("Verifying that right clicking behavior has changed"),
      MoveMouseTo(kOsSettingsWebContentsId, kCursorAcceleartorToggleQuery),
      ClickMouse(ui_controls::RIGHT),
      WaitForStateChange(kOsSettingsWebContentsId,
                         cursor_acceleration_toggle_enabled));
}

}  // namespace
}  // namespace ash

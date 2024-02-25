// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"
#include "ui/events/devices/input_device.h"

namespace ash {

namespace {

const ui::InputDevice kMouse(3, ui::InputDeviceType::INPUT_DEVICE_USB, "mouse");

class DeviceSettingsSwapPrimaryMouseButtonInteractiveUiTest
    : public DeviceSettingsBaseTest {
 public:
  DeviceSettingsSwapPrimaryMouseButtonInteractiveUiTest() {
    feature_list_.Reset();
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
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsSwapPrimaryMouseButtonInteractiveUiTest,
                       SwapPrimaryMouseButton) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kCursorAccelerationToggleEnabledEvent);
  SetMouseDevices({kMouse});
  StateChange cursor_acceleration_toggle_enabled;
  cursor_acceleration_toggle_enabled.type =
      StateChange::Type::kExistsAndConditionTrue;
  cursor_acceleration_toggle_enabled.event =
      kCursorAccelerationToggleEnabledEvent;
  cursor_acceleration_toggle_enabled.where = kCursorAcceleartorToggleQuery;
  cursor_acceleration_toggle_enabled.test_function = "el => !el.disabled";

  RunTestSequence(
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      Log("Waiting for per device mouse row to be visible"),
      WaitForElementExists(webcontents_id_, kMouseRowQuery),
      ClickElement(webcontents_id_, kMouseRowQuery),
      Log("Waiting for swap primary mouse toggle to be visible"),
      WaitForElementExists(webcontents_id_, kMouseSwapButtonDropdownQuery),
      Log("Selecting 'Right button' from the dropdown menu"),
      ExecuteJsAt(webcontents_id_, kMouseSwapButtonDropdownQuery,
                  "(el) => {el.selectedIndex = 1; el.dispatchEvent(new "
                  "Event('change'));}"),
      Log("Verifying that right clicking behavior has changed"),
      MoveMouseTo(webcontents_id_, kCursorAcceleartorToggleQuery),
      ClickMouse(ui_controls::RIGHT),
      WaitForStateChange(webcontents_id_, cursor_acceleration_toggle_enabled));
}

}  // namespace
}  // namespace ash

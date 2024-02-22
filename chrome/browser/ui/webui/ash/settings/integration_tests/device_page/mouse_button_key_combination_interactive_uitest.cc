// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

const ui::InputDevice kFiveKeyMouse(/*id=*/15,
                                    ui::INPUT_DEVICE_USB,
                                    "kFiveKeyMouse",
                                    /*phys=*/"",
                                    /*sys_path=*/base::FilePath(),
                                    /*vendor=*/0x1532,
                                    /*product=*/0x0090,
                                    /*version=*/0x0001);

// Disabled for crbug.com/325543031.
IN_PROC_BROWSER_TEST_F(DeviceSettingsBaseTest, DISABLED_MouseKeyCombination) {
  const DeepQuery kCustomizeMouseButtonsRowQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-mouse",
      "settings-per-device-mouse-subsection",
      "#customizeMouseButtons",
      "#icon",
  };

  const DeepQuery kCustomizeMouseButtonsHelpSectionQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-customize-mouse-buttons-subpage",
      "#helpSection",
  };

  const DeepQuery kCustomizeButtonsSubsectionQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "#customizeMouseButtonsRow > settings-customize-mouse-buttons-subpage",
      "#buttonsSection > customize-buttons-subsection",
  };

  const DeepQuery kRemappingActionDropdownQuery =
      kCustomizeButtonsSubsectionQuery +
      "div > customize-button-row:nth-child(1)" + "#remappingActionDropdown";

  const DeepQuery kKeyCombinationSaveQuery = kCustomizeButtonsSubsectionQuery +
                                             "key-combination-input-dialog" +
                                             "#saveButton";

  SetMouseDevices({kFiveKeyMouse});

  RunTestSequence(
      SetupInternalKeyboard(),
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kPerDeviceMouseSubpagePath),
      Log("Clicking customize mouse buttons row"),
      ClickElement(webcontents_id_, kCustomizeMouseButtonsRowQuery),
      Log("Waiting for customize mouse buttons page"),
      WaitForElementExists(webcontents_id_,
                           kCustomizeMouseButtonsHelpSectionQuery),
      Log("Registering a new button for the mouse"),

      Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        generator.PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE, kFiveKeyMouse.id);
      }),
      Log("Opening Remapping Action Dropdown"),
      ClickElement(webcontents_id_, kRemappingActionDropdownQuery),
      Log("Opening Key Combination dialog"), Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        // Select the 15th option in the dropdown menu (key combination)
        for (int i = 0; i < 15; i++) {
          generator.PressAndReleaseKey(ui::VKEY_DOWN, ui::EF_NONE, kDeviceId1);
        }
        generator.PressAndReleaseKey(ui::VKEY_RETURN, ui::EF_NONE, kDeviceId1);
      }),
      Log("Waiting for Key Combination dialog"),
      WaitForElementExists(webcontents_id_, kKeyCombinationSaveQuery),
      Log("Typing Key Combination"), Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        generator.PressAndReleaseKey(ui::VKEY_C, ui::EF_COMMAND_DOWN,
                                     kDeviceId1);
      }),
      Log("Clicking Save Button"),
      ClickElement(webcontents_id_, kKeyCombinationSaveQuery),
      Log("Navigate back one page"),
      SendAccelerator(webcontents_id_, {ui::VKEY_BROWSER_BACK, ui::EF_NONE}),
      WaitForElementExists(webcontents_id_, kCustomizeMouseButtonsRowQuery),

      Log("Check to make sure calendar is already not visible"),
      EnsureNotPresent(kCalendarViewElementId),
      Log("Activating remapped button to open calendar with Search + C"),
      Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        generator.PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE, kFiveKeyMouse.id);
      }),
      WaitForShow(kCalendarViewElementId),
      Log("Calendar opened with mouse button"), Do([&]() {
        ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
        generator.PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE, kFiveKeyMouse.id);
      }),
      WaitForHide(kCalendarViewElementId),
      Log("Calendar closed with mouse button"));
}

}  // namespace
}  // namespace ash

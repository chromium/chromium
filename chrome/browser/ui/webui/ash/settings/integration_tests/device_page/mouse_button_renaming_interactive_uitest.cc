// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_constants.h"

namespace ash {

namespace {

const ui::InputDevice kFiveKeyMouse(1,
                                    ui::INPUT_DEVICE_USB,
                                    "kFiveKeyMouse",
                                    /*phys=*/"",
                                    /*sys_path=*/base::FilePath(),
                                    /*vendor=*/0x3f0,
                                    /*product=*/0x804a,
                                    /*version=*/0x0002);

// Disabled for crbug.com/325543031.
IN_PROC_BROWSER_TEST_F(DeviceSettingsBaseTest, DISABLED_MouseButtonRenaming) {
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

  const DeepQuery kCustomizeButtonsSubsectionQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "#customizeMouseButtonsRow > settings-customize-mouse-buttons-subpage",
      "#buttonsSection > customize-buttons-subsection",
  };

  const DeepQuery kMiddleButtonEditButtonQuery =
      kCustomizeButtonsSubsectionQuery +
      "div > customize-button-row:nth-child(1)" +
      "#container > div.edit-icon-container > cr-icon-button";

  const DeepQuery kSaveButtonQuery =
      kCustomizeButtonsSubsectionQuery + "#saveButton";

  const DeepQuery kCustomizeableButtonNameQuery =
      kCustomizeButtonsSubsectionQuery +
      "div > customize-button-row:nth-child(1)" + "#buttonLabel";

  SetMouseDevices({kFiveKeyMouse});
  // Used to relaunch the settings app after the customizable mouse button
  // has been edited.
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewSettingsAppWebContentsId);

  RunTestSequence(
      SetupInternalKeyboard(),
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kPerDeviceMouseSubpagePath),
      Log("Clicking customize mouse buttons row"),
      ClickElement(webcontents_id_, kCustomizeMouseButtonsRowQuery),
      Log("Clicking edit icon for mouse 'Middle Button'"),
      ClickElement(webcontents_id_, kMiddleButtonEditButtonQuery),
      Log("Clearing existing mouse button name"),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_A, ui::EF_CONTROL_DOWN),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_BACK),
      Log("Renaming mouse button to 'custom'"), EnterLowerCaseText("custom"),
      ClickElement(webcontents_id_, kSaveButtonQuery),
      Log("Verifying that the custom mouse button has been renamed to "
          "'custom'"),
      WaitForElementTextContains(webcontents_id_, kCustomizeableButtonNameQuery,
                                 "custom"),
      Log("Closing the Settings app"),
      SendAccelerator(webcontents_id_,
                      {ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}),
      WaitForHide(webcontents_id_, /*transition_only_on_event=*/true),
      LaunchSettingsApp(kNewSettingsAppWebContentsId,
                        chromeos::settings::mojom::kPerDeviceMouseSubpagePath),
      ClickElement(kNewSettingsAppWebContentsId,
                   kCustomizeMouseButtonsRowQuery),
      Log("Confirming the updated mouse button name is saved correctly"),
      WaitForElementTextContains(kNewSettingsAppWebContentsId,
                                 kCustomizeableButtonNameQuery, "custom")

  );
}

}  // namespace
}  // namespace ash

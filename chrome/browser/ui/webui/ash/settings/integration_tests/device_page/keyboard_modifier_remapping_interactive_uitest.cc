// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/settings/integration_tests/device_page/device_settings_base_test.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

class DeviceKeyboardModifierRemappingTest : public DeviceSettingsBaseTest {
 public:
  // Query to pierce through Shadow DOM to find the touchpad row.
  const DeepQuery kKeyboardRowQuery{
      "os-settings-ui",       "os-settings-main",      "main-page-container",
      "settings-device-page", "#perDeviceKeyboardRow",
  };

  // Query to pierce through Shadow DOM to find the Settings search box.
  const DeepQuery kSearchboxQuery{
      "os-settings-ui", "settings-toolbar", "#searchBox",
      "#search",        "#searchInput",
  };

  // Query to pierce through Shadow DOM to find the Keyboard header.
  const DeepQuery kCustomizeKeyboardKeysInternalQuery{
      "os-settings-ui",
      "os-settings-main",
      "main-page-container",
      "settings-device-page",
      "settings-per-device-keyboard",
      "settings-per-device-keyboard-subsection",
      ".remap-keyboard-keys-row-internal",
  };

  const DeepQuery kCtrlDropdownQuery{
      "os-settings-ui",       "os-settings-main", "main-page-container",
      "settings-device-page", "#remap-keys",      "#ctrlKey",
      "#keyDropdown",         "#dropdownMenu",
  };

  auto WaitForSearchboxContainsText(const std::string& text) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTextFound);
    StateChange change;
    change.event = kTextFound;
    change.where = kSearchboxQuery;
    change.type = StateChange::Type::kExistsAndConditionTrue;
    const std::string value_check_function =
        base::StringPrintf("(e) => { return e.value == '%s';}", text.c_str());
    change.test_function = value_check_function;
    return WaitForStateChange(webcontents_id_, change);
  }
};

IN_PROC_BROWSER_TEST_F(DeviceKeyboardModifierRemappingTest,
                       KeyboardModifierRemapping) {
  RunTestSequence(
      Log("Adding a fake internal keyboard"), SetupInternalKeyboard(),
      LaunchSettingsApp(webcontents_id_,
                        chromeos::settings::mojom::kDeviceSectionPath),
      WaitForElementExists(webcontents_id_, kKeyboardRowQuery),
      ClickElement(webcontents_id_, kKeyboardRowQuery),
      WaitForElementTextContains(webcontents_id_, kKeyboardNameQuery,
                                 "Built-in Keyboard"),
      ClickElement(webcontents_id_, kCustomizeKeyboardKeysInternalQuery),
      Log("Remapping the 'Ctrl' key to 'Backspace'"),
      ExecuteJsAt(webcontents_id_, kCtrlDropdownQuery,
                  "(el) => {el.selectedIndex = 5; el.dispatchEvent(new "
                  "Event('change'));}"),
      ExecuteJsAt(webcontents_id_, kSearchboxQuery,
                  "(el) => { el.focus(); el.select(); }"),
      Log("Entering 'redo' into the Settings search box"),
      EnterLowerCaseText("redo"), WaitForSearchboxContainsText("redo"),
      Log("Pressing the 'Ctrl' key"),
      SendKeyPressEvent(ui::KeyboardCode::VKEY_CONTROL),
      Log("Verifying that the 'Backspace' action was performed and the search "
          "box now contains the text 'red'"),
      WaitForSearchboxContainsText("red"));
}

}  // namespace
}  // namespace ash

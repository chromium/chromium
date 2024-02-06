// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
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
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

constexpr int kDeviceId1 = 5;

class FakeDeviceManager {
 public:
  FakeDeviceManager() = default;
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() = default;

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeInternalKeyboard() {
    ui::KeyboardDevice fake_keyboard(
        /*id=*/kDeviceId1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
        /*name=*/"Keyboard1");
    fake_keyboard.sys_path = base::FilePath("path1");

    fake_keyboard_devices_.push_back(fake_keyboard);
    ui::KeyboardCapability::KeyboardInfo keyboard_info;
    keyboard_info.device_type =
        ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
    keyboard_info.top_row_layout =
        ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault;

    Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
        fake_keyboard, std::move(keyboard_info));

    // Calling RunUntilIdle() here is necessary before setting the keyboard
    // devices to prevent the callback from evdev thread to overwrite whatever
    // we set here below. See
    // `InputDeviceFactoryEvdevProxy::OnStartupScanComplete()`.
    base::RunLoop().RunUntilIdle();
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    sysfs_properties["CROS_KEYBOARD_TOP_ROW_LAYOUT"] = "1";
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/std::nullopt,
                             /*devtype=*/std::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));
  }

 private:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
};

class DeviceSettingsSixPackKeysTest : public InteractiveAshTest {
 public:
  DeviceSettingsSixPackKeysTest() {
    feature_list_.InitWithFeatures({features::kInputDeviceSettingsSplit,
                                    features::kAltClickAndSixPackCustomization},
                                   {});
  }

  auto AddFakeInternalKeyboard() {
    return Do([&]() { fake_keyboard_manager_->AddFakeInternalKeyboard(); });
  }

  auto SendKeyPressEvent(ui::KeyboardCode key) {
    return Do([key]() {
      ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
      generator.PressKey(key, ui::EF_NONE, kDeviceId1);
    });
  }

  // Enters lower-case text into the focused html input element.
  auto EnterLowerCaseText(const std::string& text) {
    return Do([&]() {
      for (char c : text) {
        ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
            .PressKey(static_cast<ui::KeyboardCode>(ui::VKEY_A + (c - 'a')),
                      ui::EF_NONE, kDeviceId1);
      }
    });
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

  // Query to pierce through Shadow DOM to find the Settings search box.
  const DeepQuery kSearchboxQuery{
      "os-settings-ui", "os-toolbar", "#searchBox", "#search", "#searchInput",
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

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();

    // Ensure the OS Settings system web app (SWA) is installed.
    InstallSystemApps();

    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

 protected:
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
  base::test::ScopedFeatureList feature_list_;
  ui::ElementIdentifier webcontents_id_;
};

IN_PROC_BROWSER_TEST_F(DeviceSettingsSixPackKeysTest, SixPackKeys) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsSettingsWebContentsId);
  webcontents_id_ = kOsSettingsWebContentsId;
  RunTestSequence(
      Log("Adding a fake internal keyboard"), AddFakeInternalKeyboard(),
      InstrumentNextTab(kOsSettingsWebContentsId, AnyBrowser()), Do([&]() {
        chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
            GetActiveUserProfile(),
            chromeos::settings::mojom::kDeviceSectionPath);
      }),
      WaitForShow(kOsSettingsWebContentsId),
      Log("Waiting for per device section to load"),
      WaitForWebContentsReady(
          kOsSettingsWebContentsId,
          chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kDeviceSectionPath)),
      WaitForElementExists(kOsSettingsWebContentsId, kKeyboardRowQuery),
      ClickElement(kOsSettingsWebContentsId, kKeyboardRowQuery),
      WaitForElementTextContains(kOsSettingsWebContentsId, kKeyboardNameQuery,
                                 "Built-in Keyboard"),
      ClickElement(kOsSettingsWebContentsId,
                   kCustomizeKeyboardKeysInternalQuery),
      Log("Remapping the 'Ctrl' key to 'Backspace'"),
      ExecuteJsAt(kOsSettingsWebContentsId, kCtrlDropdownQuery,
                  "(el) => {el.selectedIndex = 5; el.dispatchEvent(new "
                  "Event('change'));}"),
      ExecuteJsAt(kOsSettingsWebContentsId, kSearchboxQuery,
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

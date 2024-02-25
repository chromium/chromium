// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/device_keyboard_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/observer_list.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "content/public/test/test_web_ui.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"

namespace ash::settings {

namespace {

class TestKeyboardHandler : public KeyboardHandler {
 public:
  // Pull WebUIMessageHandler::set_web_ui() into public so tests can call it.
  using KeyboardHandler::set_web_ui;
};

}  // namespace

class KeyboardHandlerTest : public ChromeAshTestBase {
 public:
  KeyboardHandlerTest() : handler_test_api_(&handler_) {
    handler_.set_web_ui(&web_ui_);
    handler_.RegisterMessages();
    handler_.AllowJavascriptForTesting();

    // Make sure that we start out without any keyboards reported.
    device_data_manager_test_api_.SetKeyboardDevices({});
  }

  KeyboardHandlerTest(const KeyboardHandlerTest&) = delete;
  KeyboardHandlerTest& operator=(const KeyboardHandlerTest&) = delete;

 protected:
  // Updates out-params from the last message sent to WebUI about a change to
  // which keys should be shown. False is returned if the message was invalid or
  // not found.
  [[nodiscard]] bool GetLastShowKeysChangedMessage(
      bool* has_caps_lock_out,
      bool* has_external_meta_key_out,
      bool* has_apple_command_key_out,
      bool* has_internal_search_out,
      bool* has_assistant_key_out) {
    for (const std::unique_ptr<content::TestWebUI::CallData>& data :
         base::Reversed(web_ui_.call_data())) {
      const std::string* name = data->arg1()->GetIfString();
      if (data->function_name() != "cr.webUIListenerCallback" || !name ||
          *name != KeyboardHandler::kShowKeysChangedName) {
        continue;
      }

      if (!data->arg2() || !data->arg2()->is_dict()) {
        return false;
      }

      const base::Value::Dict& keyboard_params = data->arg2()->GetDict();
      const std::vector<std::pair<std::string, bool*>> path_to_out_param = {
          {"showCapsLock", has_caps_lock_out},
          {"showExternalMetaKey", has_external_meta_key_out},
          {"showAppleCommandKey", has_apple_command_key_out},
          {"hasLauncherKey", has_internal_search_out},
          {"hasAssistantKey", has_assistant_key_out},
      };

      for (const auto& pair : path_to_out_param) {
        auto* found = keyboard_params.Find(pair.first);
        if (!found) {
          return false;
        }

        *(pair.second) = found->GetBool();
      }

      return true;
    }
    return false;
  }

  // Returns true if the last keys-changed message reported that a Caps Lock key
  // is present and false otherwise. A failure is added if a message wasn't
  // found.
  bool HasCapsLock() {
    bool has_caps_lock = false;
    bool ignored = false;
    if (!GetLastShowKeysChangedMessage(&has_caps_lock, &ignored, &ignored,
                                       &ignored, &ignored)) {
      ADD_FAILURE() << "Didn't get " << KeyboardHandler::kShowKeysChangedName;
      return false;
    }
    return has_caps_lock;
  }

  // Returns true if the last keys-changed message reported that a Meta key on
  // an external keyboard is present and false otherwise. A failure is added if
  // a message wasn't found.
  bool HasExternalMetaKey() {
    bool has_external_meta = false;
    bool ignored = false;
    if (!GetLastShowKeysChangedMessage(&ignored, &has_external_meta, &ignored,
                                       &ignored, &ignored)) {
      ADD_FAILURE() << "Didn't get " << KeyboardHandler::kShowKeysChangedName;
      return false;
    }
    return has_external_meta;
  }

  // Returns true if the last keys-changed message reported that a Command key
  // on an Apple keyboard is present and false otherwise. A failure is added if
  // a message wasn't found.
  bool HasAppleCommandKey() {
    bool has_apple_command_key = false;
    bool ignored = false;
    if (!GetLastShowKeysChangedMessage(
            &ignored, &ignored, &has_apple_command_key, &ignored, &ignored)) {
      ADD_FAILURE() << "Didn't get " << KeyboardHandler::kShowKeysChangedName;
      return false;
    }
    return has_apple_command_key;
  }

  // Returns true if the last keys-changed message reported that the device has
  // an Launcher key remap option. This applies only to ChromeOS keyboards.
  // A failure is added if a message wasn't found.
  bool HasLauncherKey() {
    bool has_launcher_key = false;
    bool ignored = false;
    if (!GetLastShowKeysChangedMessage(&ignored, &ignored, &ignored,
                                       &has_launcher_key, &ignored)) {
      ADD_FAILURE() << "Didn't get " << KeyboardHandler::kShowKeysChangedName;
      return false;
    }
    return has_launcher_key;
  }

  // Returns true if the last keys-changed message reported that the device has
  // an assistant key on its keyboard and otherwise false. A failure is added
  // if a message wasn't found.
  bool HasAssistantKey() {
    bool has_assistant_key = false;
    bool ignored = false;
    if (!GetLastShowKeysChangedMessage(&ignored, &ignored, &ignored, &ignored,
                                       &has_assistant_key)) {
      ADD_FAILURE() << "Didn't get " << KeyboardHandler::kShowKeysChangedName;
      return false;
    }
    return has_assistant_key;
  }

  ui::DeviceDataManagerTestApi device_data_manager_test_api_;
  content::TestWebUI web_ui_;
  TestKeyboardHandler handler_;
  KeyboardHandler::TestAPI handler_test_api_;
};

TEST_F(KeyboardHandlerTest, DefaultKeys) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kHasChromeOSKeyboard);
  handler_test_api_.Initialize();
  EXPECT_FALSE(HasLauncherKey());
  EXPECT_FALSE(HasCapsLock());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());
}

TEST_F(KeyboardHandlerTest, NonChromeOSKeyboard) {
  // If kHasChromeOSKeyboard isn't passed, we should assume there's a Caps Lock
  // key.
  handler_test_api_.Initialize();
  EXPECT_FALSE(HasLauncherKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());
}

TEST_F(KeyboardHandlerTest, ExternalKeyboard) {
  auto fake_udev = std::make_unique<testing::FakeUdevLoader>();

  // Standard internal keyboard on x86 device.
  const ui::KeyboardDevice internal_kbd(
      1, ui::INPUT_DEVICE_INTERNAL, "AT Translated Set 2 keyboard", "",
      base::FilePath("/devices/platform/i8042/serio0/input/input1"), 1, 1,
      0xab41);
  fake_udev->AddFakeDevice(internal_kbd.name, internal_kbd.sys_path.value(),
                           /*subsystem=*/"input", /*devnode=*/std::nullopt,
                           /*devtype=*/std::nullopt, /*sysattrs=*/{},
                           /*properties=*/{});
  // Generic external USB keyboard.
  const ui::KeyboardDevice external_generic_kbd(
      2, ui::INPUT_DEVICE_USB, "Logitech USB Keyboard", "",
      base::FilePath("/devices/pci0000:00/0000:00:14.0/usb1/1-1/1-1:1.0/"
                     "0003:046D:C31C.0007/"
                     "input/input2"),
      0x046d, 0xc31c, 0x0111);
  fake_udev->AddFakeDevice(external_generic_kbd.name,
                           external_generic_kbd.sys_path.value(),
                           /*subsystem=*/"input", /*devnode=*/std::nullopt,
                           /*devtype=*/std::nullopt, /*sysattrs=*/{},
                           /*properties=*/{});
  // Apple keyboard.
  const ui::KeyboardDevice external_apple_kbd(
      3, ui::INPUT_DEVICE_USB, "Apple Inc. Apple Keyboard", "",
      base::FilePath("/devices/pci0000:00/0000:00:14.0/usb1/1-1/1-1:1.1/"
                     "0003:05AC:026C.000A/input/input3"),
      0x05ac, 0x026c, 0x0111);
  fake_udev->AddFakeDevice(external_apple_kbd.name,
                           external_apple_kbd.sys_path.value(),
                           /*subsystem=*/"input", /*devnode=*/std::nullopt,
                           /*devtype=*/std::nullopt, /*sysattrs=*/{},
                           /*properties=*/{});
  // Chrome OS external USB keyboard.
  const ui::KeyboardDevice external_chromeos_kbd(
      4, ui::INPUT_DEVICE_USB, "LG USB Keyboard", "",
      base::FilePath("/devices/pci0000:00/0000:00:14.0/usb1/1-1/1-1:1.0/"
                     "0003:04CA:0082.000B/input/input4"),
      0x04ca, 0x0082, 0x0111);
  fake_udev->AddFakeDevice(
      external_chromeos_kbd.name, external_chromeos_kbd.sys_path.value(),
      /*subsystem=*/"input", /*devnode=*/std::nullopt,
      /*devtype=*/std::nullopt, /*sysattrs=*/{},
      /*properties=*/{{"CROS_KEYBOARD_TOP_ROW_LAYOUT", "1"}});

  // Chrome OS external Bluetooth keyboard.
  const ui::KeyboardDevice external_bt_chromeos_kbd(
      4, ui::INPUT_DEVICE_BLUETOOTH, "LG BT Keyboard", "",
      base::FilePath("/devices/pci0000:00/0000:00:14.0/usb1/1-1/1-1:1.0/"
                     "0003:04CA:0082.000B/input/input5"),
      0x04ca, 0x0082, 0x0111);
  fake_udev->AddFakeDevice(
      external_bt_chromeos_kbd.name, external_bt_chromeos_kbd.sys_path.value(),
      /*subsystem=*/"input", /*devnode=*/std::nullopt,
      /*devtype=*/std::nullopt, /*sysattrs=*/{},
      /*properties=*/{{"CROS_KEYBOARD_TOP_ROW_LAYOUT", "1"}});

  // An internal keyboard shouldn't change the defaults.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kHasChromeOSKeyboard);
  device_data_manager_test_api_.SetKeyboardDevices({internal_kbd});
  handler_test_api_.Initialize();
  EXPECT_TRUE(HasLauncherKey());
  EXPECT_FALSE(HasCapsLock());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());

  // Simulate an external keyboard being connected. We should assume there's a
  // Caps Lock and Meta keys now.
  device_data_manager_test_api_.SetKeyboardDevices(
      std::vector<ui::KeyboardDevice>{internal_kbd, external_generic_kbd});
  EXPECT_TRUE(HasLauncherKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_TRUE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());

  // However when connecting external ChromeOS-branded keyboard, we should not
  // see neither CapsLock not meta keys.
  device_data_manager_test_api_.SetKeyboardDevices(
      std::vector<ui::KeyboardDevice>{internal_kbd, external_chromeos_kbd});
  EXPECT_TRUE(HasLauncherKey());
  EXPECT_FALSE(HasCapsLock());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());

  // Connecting external Bluetooth ChromeOS-branded keyboard, we should not
  // see neither CapsLock not meta keys.
  device_data_manager_test_api_.SetKeyboardDevices(
      std::vector<ui::KeyboardDevice>{external_bt_chromeos_kbd});
  EXPECT_TRUE(HasLauncherKey());
  EXPECT_FALSE(HasCapsLock());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());

  // Simulate an external Apple keyboard being connected. Now users can remap
  // the command key.
  device_data_manager_test_api_.SetKeyboardDevices(
      std::vector<ui::KeyboardDevice>{internal_kbd, external_apple_kbd});
  EXPECT_TRUE(HasLauncherKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_TRUE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());

  // Simulate two external keyboards (Apple and non-Apple) are connected at the
  // same time.
  device_data_manager_test_api_.SetKeyboardDevices(
      std::vector<ui::KeyboardDevice>{external_generic_kbd,
                                      external_apple_kbd});
  EXPECT_FALSE(HasLauncherKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_TRUE(HasExternalMetaKey());
  EXPECT_TRUE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());

  // Some keyboard devices don't report the string "keyboard" as part of their
  // device names. Those should also be detected as external keyboards, and
  // should show the capslock and external meta remapping.
  // https://crbug.com/834594.
  device_data_manager_test_api_.SetKeyboardDevices(
      std::vector<ui::KeyboardDevice>{
          {6, ui::INPUT_DEVICE_USB, "Topre Corporation Realforce 87", "",
           external_generic_kbd.sys_path, 0x046d, 0xc31c, 0x0111}});
  EXPECT_FALSE(HasLauncherKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_TRUE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());

  // Disconnect the external keyboard and check that the key goes away.
  device_data_manager_test_api_.SetKeyboardDevices({});
  EXPECT_FALSE(HasLauncherKey());
  EXPECT_FALSE(HasCapsLock());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
  EXPECT_FALSE(HasAssistantKey());
}

}  // namespace ash::settings

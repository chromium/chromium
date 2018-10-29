// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_keyboard_handler.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/test/test_web_ui.h"
#include "services/ws/public/cpp/input_devices/input_device_client_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device.h"

namespace chromeos {
namespace settings {

namespace {

class TestKeyboardHandler : public KeyboardHandler {
 public:
  // Pull WebUIMessageHandler::set_web_ui() into public so tests can call it.
  using KeyboardHandler::set_web_ui;
};

}  // namespace

class KeyboardHandlerTest : public testing::Test {
 public:
  KeyboardHandlerTest() : handler_test_api_(&handler_) {
    handler_.set_web_ui(&web_ui_);
    handler_.RegisterMessages();
    handler_.AllowJavascriptForTesting();

    // Make sure that we start out without any keyboards reported.
    input_device_client_test_api_.SetKeyboardDevices({});
  }

 protected:
  // Updates out-params from the last message sent to WebUI about a change to
  // which keys should be shown. False is returned if the message was invalid or
  // not found.
  bool GetLastShowKeysChangedMessage(bool* has_caps_lock_out,
                                     bool* has_diamond_key_out,
                                     bool* has_external_meta_key_out,
                                     bool* has_apple_command_key_out,
                                     bool* has_internal_search_out)
      WARN_UNUSED_RESULT {
    for (auto it = web_ui_.call_data().rbegin();
         it != web_ui_.call_data().rend(); ++it) {
      const content::TestWebUI::CallData* data = it->get();
      std::string name;
      if (data->function_name() != "cr.webUIListenerCallback" ||
          !data->arg1()->GetAsString(&name) ||
          name != KeyboardHandler::kShowKeysChangedName) {
        continue;
      }

      if (!data->arg2() ||
          data->arg2()->type() != base::Value::Type::DICTIONARY) {
        return false;
      }

      const base::Value* keyboard_params = data->arg2();
      const std::vector<std::pair<std::string, bool*>> path_to_out_param = {
          {"showCapsLock", has_caps_lock_out},
          {"showDiamondKey", has_diamond_key_out},
          {"showExternalMetaKey", has_external_meta_key_out},
          {"showAppleCommandKey", has_apple_command_key_out},
          {"hasInternalKeyboard", has_internal_search_out},
      };

      for (const auto& pair : path_to_out_param) {
        auto* found = keyboard_params->FindKey(pair.first);
        if (!found)
          return false;

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

  // Returns true if the last keys-changed message reported that a "diamond" key
  // is present and false otherwise. A failure is added if a message wasn't
  // found.
  bool HasDiamondKey() {
    bool has_diamond_key = false;
    bool ignored = false;
    if (!GetLastShowKeysChangedMessage(&ignored, &has_diamond_key, &ignored,
                                       &ignored, &ignored)) {
      ADD_FAILURE() << "Didn't get " << KeyboardHandler::kShowKeysChangedName;
      return false;
    }
    return has_diamond_key;
  }

  // Returns true if the last keys-changed message reported that a Meta key on
  // an external keyboard is present and false otherwise. A failure is added if
  // a message wasn't found.
  bool HasExternalMetaKey() {
    bool has_external_meta = false;
    bool ignored = false;
    if (!GetLastShowKeysChangedMessage(&ignored, &ignored, &has_external_meta,
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
    if (!GetLastShowKeysChangedMessage(&ignored, &ignored, &ignored,
                                       &has_apple_command_key, &ignored)) {
      ADD_FAILURE() << "Didn't get " << KeyboardHandler::kShowKeysChangedName;
      return false;
    }
    return has_apple_command_key;
  }

  // Returns true if the last keys-changed message reported that the device has
  // an internal keyboard and hence an internal Search key remap option.
  // A failure is added if a message wasn't found.
  bool HasInternalSearchKey() {
    bool has_internal_search_key = false;
    bool ignored = false;
    if (!GetLastShowKeysChangedMessage(&ignored, &ignored, &ignored, &ignored,
                                       &has_internal_search_key)) {
      ADD_FAILURE() << "Didn't get " << KeyboardHandler::kShowKeysChangedName;
      return false;
    }
    return has_internal_search_key;
  }

  ws::InputDeviceClientTestApi input_device_client_test_api_;
  content::TestWebUI web_ui_;
  TestKeyboardHandler handler_;
  KeyboardHandler::TestAPI handler_test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardHandlerTest);
};

TEST_F(KeyboardHandlerTest, DefaultKeys) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kHasChromeOSKeyboard);
  handler_test_api_.Initialize();
  EXPECT_FALSE(HasInternalSearchKey());
  EXPECT_FALSE(HasCapsLock());
  EXPECT_FALSE(HasDiamondKey());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
}

TEST_F(KeyboardHandlerTest, NonChromeOSKeyboard) {
  // If kHasChromeOSKeyboard isn't passed, we should assume there's a Caps Lock
  // key.
  handler_test_api_.Initialize();
  EXPECT_FALSE(HasInternalSearchKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_FALSE(HasDiamondKey());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
}

TEST_F(KeyboardHandlerTest, ExternalKeyboard) {
  // An internal keyboard shouldn't change the defaults.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kHasChromeOSKeyboard);
  input_device_client_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"}});
  handler_test_api_.Initialize();
  EXPECT_TRUE(HasInternalSearchKey());
  EXPECT_FALSE(HasCapsLock());
  EXPECT_FALSE(HasDiamondKey());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());

  // Simulate an external keyboard being connected. We should assume there's a
  // Caps Lock and Meta keys now.
  input_device_client_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"},
      {2, ui::INPUT_DEVICE_USB, "external keyboard"}});
  EXPECT_TRUE(HasInternalSearchKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_FALSE(HasDiamondKey());
  EXPECT_TRUE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());

  // Simulate an external Apple keyboard being connected. Now users can remap
  // the command key.
  input_device_client_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {1, ui::INPUT_DEVICE_INTERNAL, "internal keyboard"},
      {3, ui::INPUT_DEVICE_USB, "Apple Inc. Apple Keyboard"}});
  EXPECT_TRUE(HasInternalSearchKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_FALSE(HasDiamondKey());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_TRUE(HasAppleCommandKey());

  // Simulate two external keyboards (Apple and non-Apple) are connected at the
  // same time.
  input_device_client_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {2, ui::INPUT_DEVICE_USB, "external keyboard"},
      {3, ui::INPUT_DEVICE_USB, "Apple Inc. Apple Keyboard"}});
  EXPECT_FALSE(HasInternalSearchKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_FALSE(HasDiamondKey());
  EXPECT_TRUE(HasExternalMetaKey());
  EXPECT_TRUE(HasAppleCommandKey());

  // Some keyboard devices don't report the string "keyboard" as part of their
  // device names. Those should also be detcted as external keyboards, and
  // should show the capslock and external meta remapping.
  // https://crbug.com/834594.
  input_device_client_test_api_.SetKeyboardDevices(std::vector<ui::InputDevice>{
      {4, ui::INPUT_DEVICE_USB, "Topre Corporation Realforce 87"}});
  EXPECT_FALSE(HasInternalSearchKey());
  EXPECT_TRUE(HasCapsLock());
  EXPECT_FALSE(HasDiamondKey());
  EXPECT_TRUE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());

  // Disconnect the external keyboard and check that the key goes away.
  input_device_client_test_api_.SetKeyboardDevices({});
  EXPECT_FALSE(HasInternalSearchKey());
  EXPECT_FALSE(HasCapsLock());
  EXPECT_FALSE(HasDiamondKey());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
}

TEST_F(KeyboardHandlerTest, DiamondKey) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kHasChromeOSKeyboard);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kHasChromeOSDiamondKey);
  handler_test_api_.Initialize();
  EXPECT_FALSE(HasCapsLock());
  EXPECT_TRUE(HasDiamondKey());
  EXPECT_FALSE(HasExternalMetaKey());
  EXPECT_FALSE(HasAppleCommandKey());
}

}  // namespace settings
}  // namespace chromeos

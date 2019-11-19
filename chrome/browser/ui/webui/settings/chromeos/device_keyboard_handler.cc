// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_keyboard_handler.h"

#include "ash/public/cpp/keyboard_shortcut_viewer.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/mojom/constants.mojom.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/chromeos/events/keyboard_layout_util.h"

namespace {

struct KeyboardsStateResult {
  bool has_internal_keyboard = false;
  bool has_external_non_apple_keyboard = false;
  bool has_apple_keyboard = false;
};

KeyboardsStateResult GetKeyboardsState() {
  KeyboardsStateResult result;
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    result.has_internal_keyboard |=
        (keyboard.type == ui::INPUT_DEVICE_INTERNAL);

    const ui::EventRewriterChromeOS::DeviceType type =
        ui::EventRewriterChromeOS::GetDeviceType(keyboard);
    if (type == ui::EventRewriterChromeOS::kDeviceAppleKeyboard) {
      result.has_apple_keyboard = true;
    } else if (type ==
                   ui::EventRewriterChromeOS::kDeviceExternalNonAppleKeyboard ||
               type == ui::EventRewriterChromeOS::kDeviceExternalUnknown) {
      result.has_external_non_apple_keyboard = true;
    }
  }

  return result;
}

}  // namespace

namespace chromeos {
namespace settings {

const char KeyboardHandler::kShowKeysChangedName[] = "show-keys-changed";

void KeyboardHandler::TestAPI::Initialize() {
  base::ListValue args;
  handler_->HandleInitialize(&args);
}

KeyboardHandler::KeyboardHandler() = default;
KeyboardHandler::~KeyboardHandler() = default;

void KeyboardHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeKeyboardSettings",
      base::BindRepeating(&KeyboardHandler::HandleInitialize,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showKeyboardShortcutViewer",
      base::BindRepeating(&KeyboardHandler::HandleShowKeyboardShortcutViewer,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initializeKeyboardWatcher",
      base::BindRepeating(&KeyboardHandler::HandleKeyboardChange,
                          base::Unretained(this)));
}

void KeyboardHandler::OnJavascriptAllowed() {
  observer_.Add(ui::DeviceDataManager::GetInstance());
}

void KeyboardHandler::OnJavascriptDisallowed() {
  observer_.RemoveAll();
}

void KeyboardHandler::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & ui::InputDeviceEventObserver::kKeyboard) {
    AllowJavascript();
    UpdateShowKeys();
    UpdateKeyboards();
  }
}

void KeyboardHandler::HandleInitialize(const base::ListValue* args) {
  AllowJavascript();
  UpdateShowKeys();
  UpdateKeyboards();
}

void KeyboardHandler::HandleShowKeyboardShortcutViewer(
    const base::ListValue* args) const {
  ash::ToggleKeyboardShortcutViewer();
}

void KeyboardHandler::HandleKeyboardChange(const base::ListValue* args) {
  AllowJavascript();
  UpdateKeyboards();
}

void KeyboardHandler::UpdateKeyboards() {
  bool physical_keyboard = false;
  // In tablet mode, physical keybards are disabled / ignored.
  if (!ash::TabletMode::Get() || !ash::TabletMode::Get()->InTabletMode()) {
    physical_keyboard = true;
  }
  if (!physical_keyboard) {
    for (const ui::InputDevice& keyboard :
         ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
      if (keyboard.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
        physical_keyboard = true;
        break;
      }
    }
  }
  FireWebUIListener("has-hardware-keyboard", base::Value(physical_keyboard));
}

void KeyboardHandler::UpdateShowKeys() {
  // kHasChromeOSKeyboard will be unset on Chromebooks that have standalone Caps
  // Lock keys.
  const KeyboardsStateResult keyboards_state = GetKeyboardsState();
  const bool has_caps_lock = keyboards_state.has_apple_keyboard ||
                             keyboards_state.has_external_non_apple_keyboard ||
                             !base::CommandLine::ForCurrentProcess()->HasSwitch(
                                 chromeos::switches::kHasChromeOSKeyboard);

  base::Value keyboard_params(base::Value::Type::DICTIONARY);
  keyboard_params.SetKey("showCapsLock", base::Value(has_caps_lock));
  keyboard_params.SetKey(
      "showExternalMetaKey",
      base::Value(keyboards_state.has_external_non_apple_keyboard));
  keyboard_params.SetKey("showAppleCommandKey",
                         base::Value(keyboards_state.has_apple_keyboard));
  keyboard_params.SetKey("hasInternalKeyboard",
                         base::Value(keyboards_state.has_internal_keyboard));

  const bool show_assistant_key_settings = ui::DeviceKeyboardHasAssistantKey();
  keyboard_params.SetKey("hasAssistantKey",
                         base::Value(show_assistant_key_settings));

  FireWebUIListener(kShowKeysChangedName, keyboard_params);
}

}  // namespace settings
}  // namespace chromeos

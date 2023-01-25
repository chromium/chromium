// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/device_keyboard_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/keyboard_shortcut_viewer.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "content/public/browser/web_ui.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/chromeos/events/keyboard_layout_util.h"

namespace {

struct KeyboardsStateResult {
  bool has_launcher_key = false;  // ChromeOS launcher key.
  bool has_external_apple_keyboard = false;
  bool has_external_chromeos_keyboard = false;
  bool has_external_generic_keyboard = false;
};

KeyboardsStateResult GetKeyboardsState() {
  KeyboardsStateResult result;
  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    switch (ui::EventRewriterChromeOS::GetDeviceType(keyboard)) {
      case ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard:
        result.has_launcher_key = true;
        break;
      case ui::KeyboardCapability::DeviceType::kDeviceExternalAppleKeyboard:
        result.has_external_apple_keyboard = true;
        break;
      case ui::KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard:
        result.has_external_chromeos_keyboard = true;
        break;
      case ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard:
      case ui::KeyboardCapability::DeviceType::kDeviceExternalUnknown:
        result.has_external_generic_keyboard = true;
        break;
      case ui::KeyboardCapability::DeviceType::kDeviceHotrodRemote:
      case ui::KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard:
      case ui::KeyboardCapability::DeviceType::kDeviceUnknown:
        break;
    }
  }

  return result;
}

}  // namespace

namespace ash::settings {

const char KeyboardHandler::kShowKeysChangedName[] = "show-keys-changed";

void KeyboardHandler::TestAPI::Initialize() {
  handler_->HandleInitialize(base::Value::List());
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
  observation_.Observe(ui::DeviceDataManager::GetInstance());
}

void KeyboardHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void KeyboardHandler::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & ui::InputDeviceEventObserver::kKeyboard) {
    AllowJavascript();
    UpdateShowKeys();
    UpdateKeyboards();
  }
}

void KeyboardHandler::HandleInitialize(const base::Value::List& args) {
  AllowJavascript();
  UpdateShowKeys();
  UpdateKeyboards();
}

void KeyboardHandler::HandleShowKeyboardShortcutViewer(
    const base::Value::List& args) const {
  if (ash::features::ShouldOnlyShowNewShortcutApp()) {
    ash::LaunchSystemWebAppAsync(ProfileManager::GetActiveUserProfile(),
                                 ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION);
    return;
  }
  ToggleKeyboardShortcutViewer();
}

void KeyboardHandler::HandleKeyboardChange(const base::Value::List& args) {
  AllowJavascript();
  UpdateKeyboards();
}

void KeyboardHandler::UpdateKeyboards() {
  bool physical_keyboard = false;
  // In tablet mode, physical keybards are disabled / ignored.
  if (!TabletMode::Get() || !TabletMode::Get()->InTabletMode()) {
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
  const bool has_caps_lock = keyboards_state.has_external_apple_keyboard ||
                             keyboards_state.has_external_generic_keyboard ||
                             !base::CommandLine::ForCurrentProcess()->HasSwitch(
                                 switches::kHasChromeOSKeyboard);

  base::Value::Dict keyboard_params;
  keyboard_params.Set("showCapsLock", has_caps_lock);
  keyboard_params.Set("showExternalMetaKey",
                      keyboards_state.has_external_generic_keyboard);
  keyboard_params.Set("showAppleCommandKey",
                      keyboards_state.has_external_apple_keyboard);
  // An external (USB/BT) ChromeOS keyboard is treated similarly to an internal
  // ChromeOS keyboard. i.e. they are functionally the same.
  keyboard_params.Set("hasLauncherKey",
                      keyboards_state.has_launcher_key ||
                          keyboards_state.has_external_chromeos_keyboard);

  const bool show_assistant_key_settings = ui::DeviceKeyboardHasAssistantKey();
  keyboard_params.Set("hasAssistantKey", show_assistant_key_settings);

  FireWebUIListener(kShowKeysChangedName, keyboard_params);
}

}  // namespace ash::settings

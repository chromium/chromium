// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/a11y/switch_access_handler.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace ash::settings {
namespace {

struct AssignmentInfo {
  std::string action_name_for_js;
  std::string pref_name;
};

std::string GetStringForKeyboardCode(ui::KeyboardCode key_code) {
  ui::DomKey dom_key;
  ui::KeyboardCode key_code_to_compare = ui::VKEY_UNKNOWN;
  for (const auto& dom_code : ui::kDomCodesArray) {
    if (!ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()->Lookup(
            dom_code, /*flags=*/ui::EF_NONE, &dom_key, &key_code_to_compare)) {
      continue;
    }
    if (key_code_to_compare != key_code || !dom_key.IsValid() ||
        dom_key.IsDeadKey()) {
      continue;
    }
    // Make sure the space key is rendered as "Space" instead of " ".
    if (key_code == ui::VKEY_SPACE) {
      return l10n_util::GetStringUTF8(IDS_SETTINGS_SWITCH_ASSIGN_OPTION_SPACE);
    }
    return ui::KeycodeConverter::DomKeyToKeyString(dom_key);
  }
  return std::string();
}

std::string GetSwitchAccessDevice(ui::InputDeviceType source_device_type) {
  switch (source_device_type) {
    case ui::INPUT_DEVICE_INTERNAL:
      return kSwitchAccessInternalDevice;
    case ui::INPUT_DEVICE_USB:
      return kSwitchAccessUsbDevice;
    case ui::INPUT_DEVICE_BLUETOOTH:
      return kSwitchAccessBluetoothDevice;
    case ui::INPUT_DEVICE_UNKNOWN:
      return kSwitchAccessUnknownDevice;
  }
}

}  // namespace

SwitchAccessHandler::SwitchAccessHandler(PrefService* prefs) : prefs_(prefs) {}

SwitchAccessHandler::~SwitchAccessHandler() {
  // Ensure we always leave Switch Access in a good state no matter what.
  if (web_ui() && web_ui()->GetWebContents() &&
      web_ui()->GetWebContents()->GetNativeView()) {
    web_ui()->GetWebContents()->GetNativeView()->RemovePreTargetHandler(this);
  }

  if (AccessibilityController::Get()) {
    AccessibilityController::Get()->SuspendSwitchAccessKeyHandling(false);
  }
}

void SwitchAccessHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "refreshAssignmentsFromPrefs",
      base::BindRepeating(
          &SwitchAccessHandler::HandleRefreshAssignmentsFromPrefs,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifySwitchAccessActionAssignmentPaneActive",
      base::BindRepeating(
          &SwitchAccessHandler::
              HandleNotifySwitchAccessActionAssignmentPaneActive,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifySwitchAccessActionAssignmentPaneInactive",
      base::BindRepeating(
          &SwitchAccessHandler::
              HandleNotifySwitchAccessActionAssignmentPaneInactive,
          base::Unretained(this)));
}

void SwitchAccessHandler::AddPreTargetHandler() {
  CHECK(IsJavascriptAllowed());

  if (on_pre_target_handler_added_for_testing_) {
    std::move(on_pre_target_handler_added_for_testing_).Run();
  }
  if (skip_pre_target_handler_for_testing_) {
    return;
  }

  gfx::NativeView native_view = web_ui()->GetWebContents()->GetNativeView();

  // Ensure we don't add the handler twice.
  native_view->RemovePreTargetHandler(this);
  native_view->AddPreTargetHandler(this);
}

void SwitchAccessHandler::OnJavascriptAllowed() {
  if (action_assignment_pane_active_) {
    AddPreTargetHandler();
  }

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs_);
  pref_change_registrar_->Add(
      ash::prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes,
      base::BindRepeating(
          &SwitchAccessHandler::OnSwitchAccessAssignmentsUpdated,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes,
      base::BindRepeating(
          &SwitchAccessHandler::OnSwitchAccessAssignmentsUpdated,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes,
      base::BindRepeating(
          &SwitchAccessHandler::OnSwitchAccessAssignmentsUpdated,
          base::Unretained(this)));
}

void SwitchAccessHandler::OnJavascriptDisallowed() {
  web_ui()->GetWebContents()->GetNativeView()->RemovePreTargetHandler(this);
  pref_change_registrar_.reset();
}

void SwitchAccessHandler::OnKeyEvent(ui::KeyEvent* event) {
  CHECK(action_assignment_pane_active_);
  event->StopPropagation();
  event->SetHandled();

  if (event->type() == ui::EventType::kKeyReleased) {
    return;
  }

  base::Value::Dict response;
  response.Set("keyCode", static_cast<int>(event->key_code()));
  response.Set("key", GetStringForKeyboardCode(event->key_code()));
  ui::InputDeviceType deviceType = ui::INPUT_DEVICE_UNKNOWN;

  int source_device_id = event->source_device_id();
  for (const auto& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (source_device_id == keyboard.id) {
      deviceType = keyboard.type;
      break;
    }
  }
  response.Set("device", GetSwitchAccessDevice(deviceType));

  FireWebUIListener("switch-access-got-key-press-for-assignment", response);
}

void SwitchAccessHandler::HandleRefreshAssignmentsFromPrefs(
    const base::Value::List& args) {
  AllowJavascript();
  OnSwitchAccessAssignmentsUpdated();
}

void SwitchAccessHandler::HandleNotifySwitchAccessActionAssignmentPaneActive(
    const base::Value::List& args) {
  action_assignment_pane_active_ = true;
  if (!IsJavascriptAllowed()) {
    AllowJavascript();
  } else {
    AddPreTargetHandler();
  }
  OnSwitchAccessAssignmentsUpdated();
  AccessibilityController::Get()->SuspendSwitchAccessKeyHandling(true);
}

void SwitchAccessHandler::HandleNotifySwitchAccessActionAssignmentPaneInactive(
    const base::Value::List& args) {
  action_assignment_pane_active_ = false;
  if (!skip_pre_target_handler_for_testing_) {
    web_ui()->GetWebContents()->GetNativeView()->RemovePreTargetHandler(this);
  }
  AccessibilityController::Get()->SuspendSwitchAccessKeyHandling(false);
}

void SwitchAccessHandler::OnSwitchAccessAssignmentsUpdated() {
  base::Value::Dict response;

  static base::NoDestructor<std::vector<AssignmentInfo>> kAssignmentInfo({
      {"select", ash::prefs::kAccessibilitySwitchAccessSelectDeviceKeyCodes},
      {"next", ash::prefs::kAccessibilitySwitchAccessNextDeviceKeyCodes},
      {"previous",
       ash::prefs::kAccessibilitySwitchAccessPreviousDeviceKeyCodes},
  });

  for (const AssignmentInfo& info : *kAssignmentInfo) {
    const auto& keycodes = prefs_->GetDict(info.pref_name);
    base::Value::List keys;
    for (const auto item : keycodes) {
      int key_code;
      if (!base::StringToInt(item.first, &key_code)) {
        NOTREACHED_IN_MIGRATION();
        return;
      }
      for (const base::Value& device_type : item.second.GetList()) {
        base::Value::Dict key;
        key.Set("key", GetStringForKeyboardCode(
                           static_cast<ui::KeyboardCode>(key_code)));
        key.Set("device", device_type.GetString());
        keys.Append(std::move(key));
      }
    }
    response.SetByDottedPath(info.action_name_for_js, std::move(keys));
  }

  FireWebUIListener("switch-access-assignments-changed", response);
}

}  // namespace ash::settings

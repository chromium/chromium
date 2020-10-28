// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/switch_access_handler.h"

#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_codes.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace chromeos {
namespace settings {
namespace {

struct AssignmentInfo {
  std::string action_name_for_js;
  std::string pref_name;
};

std::string GetStringForKeyboardCode(ui::KeyboardCode key_code) {
  ui::DomKey dom_key;
  ui::KeyboardCode key_code_to_compare = ui::VKEY_UNKNOWN;
  for (const auto& dom_code : ui::dom_codes) {
    if (!ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()->Lookup(
            dom_code, /*flags=*/ui::EF_NONE, &dom_key, &key_code_to_compare)) {
      continue;
    }
    if (key_code_to_compare != key_code || !dom_key.IsValid() ||
        dom_key.IsDeadKey()) {
      continue;
    }
    return ui::KeycodeConverter::DomKeyToKeyString(dom_key);
  }
  return std::string();
}

}  // namespace

SwitchAccessHandler::SwitchAccessHandler(PrefService* prefs) : prefs_(prefs) {}

SwitchAccessHandler::~SwitchAccessHandler() {
  // Ensure we always leave Switch Access in a good state no matter what.
  if (web_ui() && web_ui()->GetWebContents() &&
      web_ui()->GetWebContents()->GetNativeView()) {
    web_ui()->GetWebContents()->GetNativeView()->RemovePreTargetHandler(this);
  }

  if (ash::AccessibilityController::Get())
    ash::AccessibilityController::Get()->SuspendSwitchAccessKeyHandling(false);
}

void SwitchAccessHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "refreshAssignmentsFromPrefs",
      base::BindRepeating(
          &SwitchAccessHandler::HandleRefreshAssignmentsFromPrefs,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifySwitchAccessActionAssignmentDialogAttached",
      base::BindRepeating(
          &SwitchAccessHandler::
              HandleNotifySwitchAccessActionAssignmentDialogAttached,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifySwitchAccessActionAssignmentDialogDetached",
      base::BindRepeating(
          &SwitchAccessHandler::
              HandleNotifySwitchAccessActionAssignmentDialogDetached,
          base::Unretained(this)));
}

void SwitchAccessHandler::OnJavascriptAllowed() {
  pref_change_registrar_.reset(new PrefChangeRegistrar);
  pref_change_registrar_->Init(prefs_);
  pref_change_registrar_->Add(
      ash::prefs::kAccessibilitySwitchAccessSelectKeyCodes,
      base::BindRepeating(
          &SwitchAccessHandler::OnSwitchAccessAssignmentsUpdated,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kAccessibilitySwitchAccessNextKeyCodes,
      base::BindRepeating(
          &SwitchAccessHandler::OnSwitchAccessAssignmentsUpdated,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kAccessibilitySwitchAccessPreviousKeyCodes,
      base::BindRepeating(
          &SwitchAccessHandler::OnSwitchAccessAssignmentsUpdated,
          base::Unretained(this)));
}

void SwitchAccessHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.reset();
}

void SwitchAccessHandler::OnKeyEvent(ui::KeyEvent* event) {
  event->StopPropagation();
  event->SetHandled();

  if (event->type() == ui::ET_KEY_RELEASED)
    return;

  base::DictionaryValue response;
  response.SetIntPath("keyCode", static_cast<int>(event->key_code()));
  response.SetStringPath("key", GetStringForKeyboardCode(event->key_code()));

  // TODO(accessibility): also include the device type once Switch Access can
  // distinguish between internal, usb, and bluetooth keyboards for each action
  // type.

  FireWebUIListener("switch-access-got-key-press-for-assignment", response);
}

void SwitchAccessHandler::HandleRefreshAssignmentsFromPrefs(
    const base::ListValue* args) {
  AllowJavascript();
  OnSwitchAccessAssignmentsUpdated();
}

void SwitchAccessHandler::
    HandleNotifySwitchAccessActionAssignmentDialogAttached(
        const base::ListValue* args) {
  OnSwitchAccessAssignmentsUpdated();
  web_ui()->GetWebContents()->GetNativeView()->AddPreTargetHandler(this);
  ash::AccessibilityController::Get()->SuspendSwitchAccessKeyHandling(true);
}

void SwitchAccessHandler::
    HandleNotifySwitchAccessActionAssignmentDialogDetached(
        const base::ListValue* args) {
  web_ui()->GetWebContents()->GetNativeView()->RemovePreTargetHandler(this);
  ash::AccessibilityController::Get()->SuspendSwitchAccessKeyHandling(false);
}

void SwitchAccessHandler::OnSwitchAccessAssignmentsUpdated() {
  base::DictionaryValue response;

  static base::NoDestructor<std::vector<AssignmentInfo>> kAssignmentInfo({
      {"select", ash::prefs::kAccessibilitySwitchAccessSelectKeyCodes},
      {"next", ash::prefs::kAccessibilitySwitchAccessNextKeyCodes},
      {"previous", ash::prefs::kAccessibilitySwitchAccessPreviousKeyCodes},
  });

  for (const AssignmentInfo& info : *kAssignmentInfo) {
    auto* keycodes = prefs_->GetList(info.pref_name);
    base::ListValue keys;
    for (size_t i = 0; i < keycodes->GetList().size(); i++) {
      keys.Append(GetStringForKeyboardCode(
          static_cast<ui::KeyboardCode>(keycodes->GetList()[i].GetInt())));
    }
    response.SetPath(info.action_name_for_js, std::move(keys));
  }

  FireWebUIListener("switch-access-assignments-changed", response);
}

}  // namespace settings
}  // namespace chromeos

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/omnibox_everywhere_settings_handler.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace settings {

OmniboxEverywhereSettingsHandler::OmniboxEverywhereSettingsHandler() = default;

OmniboxEverywhereSettingsHandler::~OmniboxEverywhereSettingsHandler() {
  SetShortcutSuspensionState(false);
}

void OmniboxEverywhereSettingsHandler::RegisterMessages() {
  // Handler to fetch the current shortcut display string when settings load.
  web_ui()->RegisterMessageCallback(
      "getOmniboxEverywhereShortcut",
      base::BindRepeating(
          &OmniboxEverywhereSettingsHandler::HandleGetOmniboxEverywhereShortcut,
          base::Unretained(this)));

  // Handler to save the newly recorded shortcut to Local State.
  web_ui()->RegisterMessageCallback(
      "setOmniboxEverywhereShortcut",
      base::BindRepeating(
          &OmniboxEverywhereSettingsHandler::HandleSetOmniboxEverywhereShortcut,
          base::Unretained(this)));

  // Handler to suspend/resume global hotkey interception during shortcut input.
  web_ui()->RegisterMessageCallback(
      "setOmniboxEverywhereShortcutSuspensionState",
      base::BindRepeating(&OmniboxEverywhereSettingsHandler::
                              HandleSetOmniboxEverywhereShortcutSuspensionState,
                          base::Unretained(this)));
}

void OmniboxEverywhereSettingsHandler::OnJavascriptDisallowed() {
  SetShortcutSuspensionState(false);
}

void OmniboxEverywhereSettingsHandler::HandleGetOmniboxEverywhereShortcut(
    const base::ListValue& args) {
  AllowJavascript();
  if (args.empty() || !args[0].is_string()) {
    return;
  }
  const base::Value& callback_id = args[0];

  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  const ui::Accelerator shortcut =
      omnibox_everywhere::prefs::GetOmniboxEverywhereHotkey(local_state);

  // Return the localized accelerator text (e.g. "Alt+Space" or
  // "Ctrl+Shift+Space").
  ResolveJavascriptCallback(
      callback_id, base::Value(base::UTF16ToUTF8(shortcut.GetShortcutText())));
}

void OmniboxEverywhereSettingsHandler::HandleSetOmniboxEverywhereShortcut(
    const base::ListValue& args) {
  AllowJavascript();
  if (args.size() < 2 || !args[0].is_string() || !args[1].is_string()) {
    return;
  }
  const base::Value& callback_id = args[0];
  const std::string& accelerator_string = args[1].GetString();

  bool is_valid = false;
  if (accelerator_string.empty()) {
    is_valid = true;
  } else {
    const ui::Accelerator accelerator =
        ui::Command::StringToAccelerator(accelerator_string);
    // A valid shortcut must have at least one modifier key.
    if (!accelerator.IsEmpty() &&
        ui::Accelerator::MaskOutKeyEventFlags(accelerator.modifiers()) != 0) {
      is_valid = true;
    }
  }

  if (is_valid) {
    PrefService* local_state =
        g_browser_process ? g_browser_process->local_state() : nullptr;
    if (local_state) {
      // Write user-entered accelerator string into Local State pref.
      local_state->SetString(
          omnibox_everywhere::prefs::kOmniboxEverywhereHotkey,
          accelerator_string);
    }
  }

  ResolveJavascriptCallback(callback_id, base::Value(is_valid));
}

void OmniboxEverywhereSettingsHandler::
    HandleSetOmniboxEverywhereShortcutSuspensionState(
        const base::ListValue& args) {
  if (args.empty() || !args[0].is_bool()) {
    return;
  }
  SetShortcutSuspensionState(args[0].GetBool());
}

void OmniboxEverywhereSettingsHandler::SetShortcutSuspensionState(
    bool suspend) {
  if (is_shortcut_suspended_ == suspend) {
    return;
  }
  is_shortcut_suspended_ = suspend;
  // Suspend global shortcut handling when WebUI is actively capturing keys so
  // that typing combinations in the UI does not trigger global listeners.
  if (auto* const global_accelerator_listener =
          ui::GlobalAcceleratorListener::GetInstance()) {
    global_accelerator_listener->SetShortcutHandlingSuspended(suspend);
  }
}

}  // namespace settings

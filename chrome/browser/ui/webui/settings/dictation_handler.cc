// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/dictation_handler.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"

namespace settings {

DictationHandler::DictationHandler() = default;
DictationHandler::~DictationHandler() = default;

void DictationHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDictationShortcut",
      base::BindRepeating(&DictationHandler::HandleGetDictationShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDictationShortcut",
      base::BindRepeating(&DictationHandler::HandleSetDictationShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDictationPressAndHoldShortcut",
      base::BindRepeating(
          &DictationHandler::HandleGetDictationPressAndHoldShortcut,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDictationPressAndHoldShortcut",
      base::BindRepeating(
          &DictationHandler::HandleSetDictationPressAndHoldShortcut,
          base::Unretained(this)));
}

void DictationHandler::HandleGetDictationShortcut(const base::ListValue& args) {
  HandleGetShortcutForPref(args, prefs::kVoiceTypingHotkey);
}

void DictationHandler::HandleSetDictationShortcut(const base::ListValue& args) {
  HandleSetShortcutForPref(args, prefs::kVoiceTypingHotkey);
}

void DictationHandler::HandleGetDictationPressAndHoldShortcut(
    const base::ListValue& args) {
  HandleGetShortcutForPref(args, prefs::kVoiceTypingPressAndHoldHotkey);
}

void DictationHandler::HandleSetDictationPressAndHoldShortcut(
    const base::ListValue& args) {
  HandleSetShortcutForPref(args, prefs::kVoiceTypingPressAndHoldHotkey);
}

void DictationHandler::HandleGetShortcutForPref(const base::ListValue& args,
                                                std::string_view pref_name) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  AllowJavascript();

  std::string pref_shortcut =
      Profile::FromWebUI(web_ui())->GetPrefs()->GetString(pref_name);
  ui::Accelerator accelerator = ui::Command::StringToAccelerator(pref_shortcut);

  ResolveJavascriptCallback(
      callback_id,
      base::Value(base::UTF16ToUTF8(accelerator.GetShortcutText())));
}

void DictationHandler::HandleSetShortcutForPref(const base::ListValue& args,
                                                std::string_view pref_name) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const base::Value& callback_id = args[0];
  const std::string accelerator_string = args[1].GetString();

  bool is_valid = false;
  if (accelerator_string.empty()) {
    is_valid = true;
  } else {
    ui::Accelerator accelerator =
        ui::Command::StringToAccelerator(accelerator_string);
    // A valid shortcut must have at least one modifier key.
    if (!accelerator.IsEmpty() &&
        ui::Accelerator::MaskOutKeyEventFlags(accelerator.modifiers()) != 0) {
      is_valid = true;
    }
  }

  if (is_valid) {
    Profile::FromWebUI(web_ui())->GetPrefs()->SetString(pref_name,
                                                        accelerator_string);
  }

  ResolveJavascriptCallback(callback_id, base::Value(is_valid));
}

}  // namespace settings

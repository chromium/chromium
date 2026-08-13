// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/dictation_handler.h"

#include <string>

#include "base/functional/bind.h"
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
      "setDictationShortcut",
      base::BindRepeating(&DictationHandler::HandleSetDictationShortcut,
                          base::Unretained(this)));
}

void DictationHandler::HandleSetDictationShortcut(const base::ListValue& args) {
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
    Profile::FromWebUI(web_ui())->GetPrefs()->SetString(
        prefs::kVoiceTypingHotkey, accelerator_string);
  }

  ResolveJavascriptCallback(callback_id, base::Value(is_valid));
}

}  // namespace settings

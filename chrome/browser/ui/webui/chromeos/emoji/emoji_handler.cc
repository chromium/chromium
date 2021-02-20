// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_handler.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_dialog.h"
#include "ui/aura/window.h"
#include "ui/base/ime/chromeos/ime_bridge.h"

namespace chromeos {

EmojiHandler::EmojiHandler() {}
EmojiHandler::~EmojiHandler() = default;

// Keep in sync with entry in enums.xml.
enum class EmojiVariantType {
  // smaller entries only used by Chrome OS VK
  kEmojiPickerBase = 4,
  kEmojiPickerVariant = 5,
  kMaxValue = kEmojiPickerVariant,
};

void LogInsertEmoji(bool isVariant) {
  EmojiVariantType insertValue = isVariant
                                     ? EmojiVariantType::kEmojiPickerVariant
                                     : EmojiVariantType::kEmojiPickerBase;
  base::UmaHistogramEnumeration("InputMethod.VirtualKeyboard.Emoji.TriggerType",
                                insertValue);
}

void EmojiHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "insertEmoji", base::BindRepeating(&EmojiHandler::HandleInsertEmoji,
                                         base::Unretained(this)));
}

void EmojiHandler::HandleInsertEmoji(const base::ListValue* args) {
  if (args->GetSize() != 2) {
    DLOG(WARNING)
        << "insertEmoji called with incorrect number of arguments (expected 2)";
    return;
  }

  const std::string& emoji = args->GetList()[0].GetString();
  LogInsertEmoji(args->GetList()[1].GetBool());

  // Hide emoji picker window to restore focus to original text field
  if (EmojiPickerDialog::window) {
    EmojiPickerDialog::window->Hide();
    EmojiPickerDialog::window = nullptr;
  }

  ui::InputMethod* input_method =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method) {
    LOG(WARNING) << "no input_method found";
    return;
  }

  ui::TextInputClient* input_client = input_method->GetTextInputClient();

  if (!input_client) {
    LOG(WARNING) << "no input_client found";
    return;
  }

  if (input_client->GetTextInputType() ==
      ui::TextInputType::TEXT_INPUT_TYPE_NONE) {
    LOG(WARNING) << "attempt to insert into input_client with type none";
    return;
  }

  input_client->InsertText(
      base::UTF8ToUTF16(emoji),
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
}

}  // namespace chromeos

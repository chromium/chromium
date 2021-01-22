// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_handler.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_dialog.h"

namespace chromeos {

EmojiHandler::EmojiHandler() : selection_range_set(false) {}
EmojiHandler::~EmojiHandler() = default;

void EmojiHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "insertEmoji", base::BindRepeating(&EmojiHandler::HandleInsertEmoji,
                                         base::Unretained(this)));
}

void EmojiHandler::HandleInsertEmoji(const base::ListValue* args) {
  if (args->GetSize() != 1) {
    DLOG(WARNING)
        << "insertEmoji called with incorrect number of arguments (expected 1)";
    return;
  }

  const std::string& emoji = args->GetList()[0].GetString();

  ui::TextInputClient* input_client = EmojiPickerDialog::input_client;

  if (!input_client) {
    LOG(WARNING) << "no input_client found";
    return;
  }

  if (input_client->GetTextInputType() ==
      ui::TextInputType::TEXT_INPUT_TYPE_NONE) {
    LOG(WARNING) << "attempt to insert into input_client with type none";
  }

  if (!selection_range_set) {
    input_client->SetEditableSelectionRange(EmojiPickerDialog::selection_range);
    selection_range_set = true;
  }

  input_client->InsertText(
      base::UTF8ToUTF16(emoji),
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
}

}  // namespace chromeos

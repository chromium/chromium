// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_handler.h"

#include "base/logging.h"

namespace chromeos {

EmojiHandler::EmojiHandler() = default;
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

  // Example usage:
  // const std::string& emoji = args->GetList()[0].GetString();
}

}  // namespace chromeos
